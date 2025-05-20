//! Fish supports multiple shells writing to history at once. Here is its strategy:
//!
//! 1. All history files are append-only. Data, once written, is never modified.
//!
//! 2. A history file may be re-written ("vacuumed"). This involves reading in the file and writing a
//! new one, while performing maintenance tasks: discarding items in an LRU fashion until we reach
//! the desired maximum count, removing duplicates, and sorting them by timestamp (eventually, not
//! implemented yet). The new file is atomically moved into place via rename().
//!
//! 3. History files are mapped in via mmap(). Before the file is mapped, the file takes a fcntl read
//! lock. The purpose of this lock is to avoid seeing a transient state where partial data has been
//! written to the file.
//!
//! 4. History is appended to under a fcntl write lock.
//!
//! 5. The chaos_mode boolean can be set to true to do things like lower buffer sizes which can
//! trigger race conditions. This is useful for testing.
//!
//! Locking on remote filesystems may hang for an unacceptably long time. For that reason, fish
//! does not take locks on the file if it believes the history file is on a remote filesystem,
//! or if the mmap fails with ENODEV, or if the first lock attempt takes excessively long.
//! Eliding locks means that two concurrent shell sessions with a remote history file may, in
//! rare cases with multiple simultaneous shell sessions, lose a history item; this is
//! considered preferable to hanging the the shell waiting for a lock.

use crate::{
    common::cstr2wcstring, env::EnvVar, wcstringutil::trim,
    wutil::fileid::file_id_for_path_or_error,
};
use std::{
    borrow::Cow,
    collections::{BTreeMap, HashMap, HashSet},
    ffi::CString,
    fs::File,
    io::{BufRead, Read, Seek, SeekFrom, Write},
    mem::MaybeUninit,
    num::NonZeroUsize,
    ops::ControlFlow,
    os::{fd::AsRawFd, unix::fs::MetadataExt},
    sync::{Arc, Mutex, MutexGuard},
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use bitflags::bitflags;
use libc::{fchown, flock, LOCK_EX, LOCK_SH, LOCK_UN};
use lru::LruCache;
use nix::{fcntl::OFlag, sys::stat::Mode};
use rand::Rng;

use crate::{
    ast::{self, Kind, Node},
    common::{
        str2wcstring, unescape_string, valid_var_name, wcs2zstring, CancelChecker,
        UnescapeStringStyle,
    },
    env::{EnvMode, EnvStack, Environment},
    expand::{expand_one, ExpandFlags},
    fallback::fish_mkstemp_cloexec,
    fds::wopen_cloexec,
    flog::{FLOG, FLOGF},
    global_safety::RelaxedAtomicBool,
    history::file::{append_history_item_to_buffer, HistoryFileContents},
    io::IoStreams,
    operation_context::{OperationContext, EXPANSION_LIMIT_BACKGROUND},
    parse_constants::{ParseTreeFlags, StatementDecoration},
    parse_util::{parse_util_detect_errors, parse_util_unescape_wildcards},
    path::{
        path_get_config, path_get_data, path_get_data_remoteness, path_is_valid, DirRemoteness,
    },
    threads::{assert_is_background_thread, iothread_perform},
    util::{find_subslice, get_rng},
    wchar::prelude::*,
    wcstringutil::subsequence_in_string,
    wildcard::{wildcard_match, ANY_STRING},
    wutil::{
        file_id_for_file, file_id_for_path, wgettext_fmt, wrealpath, wrename, wstat, wunlink,
        FileId, INVALID_FILE_ID,
    },
};

mod file;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SearchType {
    /// Search for commands exactly matching the given string.
    Exact,
    /// Search for commands containing the given string.
    Contains,
    /// Search for commands starting with the given string.
    Prefix,
    /// Search for commands where any line matches the given string.
    LinePrefix,
    /// Search for commands containing the given glob pattern.
    ContainsGlob,
    /// Search for commands starting with the given glob pattern.
    PrefixGlob,
    /// Search for commands containing the given string as a subsequence
    ContainsSubsequence,
    /// Matches everything.
    MatchEverything,
}

/// Ways that a history item may be written to disk (or omitted).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PersistenceMode {
    /// The history item is written to disk normally
    Disk,
    /// The history item is stored in-memory only, not written to disk
    Memory,
    /// The history item is stored in-memory and deleted when a new item is added
    Ephemeral,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SearchDirection {
    Forward,
    Backward,
}

use self::file::time_to_seconds;

// Our history format is intended to be valid YAML. Here it is:
//
//   - cmd: ssh blah blah blah
//     when: 2348237
//     paths:
//       - /path/to/something
//       - /path/to/something_else
//
//   Newlines are replaced by \n. Backslashes are replaced by \\.

/// This is the history session ID we use by default if the user has not set env var fish_history.
const DFLT_FISH_HISTORY_SESSION_ID: &wstr = L!("fish");

/// When we rewrite the history, the number of items we keep.
// FIXME: https://github.com/rust-lang/rust/issues/67441
const HISTORY_SAVE_MAX: NonZeroUsize = unsafe { NonZeroUsize::new_unchecked(1024 * 256) };

/// Default buffer size for flushing to the history file.
const HISTORY_OUTPUT_BUFFER_SIZE: usize = 64 * 1024;

/// The file access mode we use for creating history files
const HISTORY_FILE_MODE: Mode = Mode::from_bits_truncate(0o600);

/// How many times we retry to save
/// Saving may fail if the file is modified in between our opening
/// the file and taking the lock
const MAX_SAVE_TRIES: usize = 1024;

pub const VACUUM_FREQUENCY: usize = 25;

/// Output the contents `buffer` to `file` and clear the `buffer`.
fn drain_buffer_into_file_no_flush(buffer: &mut Vec<u8>, file: &mut File) -> std::io::Result<()> {
    file.write_all(buffer)?;
    buffer.clear();
    Ok(())
}

/// Output the contents `buffer` to `file` and clear the `buffer`.
/// Flush the file and sync it to ensure that the updates actually reach the file system.
fn drain_buffer_into_file_and_flush(buffer: &mut Vec<u8>, file: &mut File) -> std::io::Result<()> {
    drain_buffer_into_file_no_flush(buffer, file)?;
    file.flush()?;
    file.sync_all()
}

struct TimeProfiler {
    what: &'static str,
    start: SystemTime,
}

impl TimeProfiler {
    fn new(what: &'static str) -> Self {
        let start = SystemTime::now();
        Self { what, start }
    }
}

impl Drop for TimeProfiler {
    fn drop(&mut self) {
        if let Ok(duration) = self.start.elapsed() {
            let ns_per_ms = 1_000_000;
            let ms = duration.as_millis();
            let ns = duration.as_nanos() - (ms * ns_per_ms);
            FLOGF!(
                profile_history,
                "%s: %d.%06d ms",
                self.what,
                ms as u64, // todo!("remove cast")
                ns as u32
            )
        } else {
            FLOGF!(profile_history, "%s: ??? ms", self.what)
        }
    }
}

trait LruCacheExt {
    /// Function to add a history item.
    fn add_item(&mut self, item: HistoryItem);
}

impl LruCacheExt for LruCache<WString, HistoryItem> {
    fn add_item(&mut self, item: HistoryItem) {
        // Skip empty items.
        if item.is_empty() {
            return;
        }

        // See if it's in the cache. If it is, update the timestamp. If not, we create a new node
        // and add it. Note that calling get_node promotes the node to the front.
        let key = item.str();
        if let Some(node) = self.get_mut(key) {
            node.creation_timestamp = SystemTime::max(node.timestamp(), item.timestamp());
            // What to do about paths here? Let's just ignore them.
        } else {
            self.put(key.to_owned(), item);
        }
    }
}

/// Returns the path for the history file for the given `session_id`, or `None` if it could not be
/// loaded. If `suffix` is provided, append that suffix to the path; this is used for temporary files.
fn history_filename(session_id: &wstr, suffix: &wstr) -> Option<WString> {
    if session_id.is_empty() {
        return None;
    }

    let mut result = path_get_data()?;

    result.push('/');
    result.push_utfstr(session_id);
    result.push_utfstr(L!("_history"));
    result.push_utfstr(suffix);
    Some(result)
}

pub type PathList = Vec<WString>;
pub type HistoryIdentifier = u64;

#[derive(Clone, Debug)]
pub struct HistoryItem {
    /// The actual contents of the entry.
    contents: WString,
    /// Original creation time for the entry.
    creation_timestamp: SystemTime,
    /// Paths that we require to be valid for this item to be autosuggested.
    required_paths: Vec<WString>,
    /// Sometimes unique identifier used for hinting.
    identifier: HistoryIdentifier,
    /// Whether to write this item to disk.
    persist_mode: PersistenceMode,
}

impl HistoryItem {
    /// Construct from a text, timestamp, and optional identifier.
    /// If `persist_mode` is not [`PersistenceMode::Disk`], then do not write this item to disk.
    pub fn new(
        s: WString,
        when: SystemTime,              /*=0*/
        ident: HistoryIdentifier,      /*=0*/
        persist_mode: PersistenceMode, /*=Disk*/
    ) -> Self {
        Self {
            contents: s,
            creation_timestamp: when,
            required_paths: vec![],
            identifier: ident,
            persist_mode,
        }
    }

    /// Returns the text as a string.
    pub fn str(&self) -> &wstr {
        &self.contents
    }

    /// Returns whether the text is empty.
    pub fn is_empty(&self) -> bool {
        self.contents.is_empty()
    }

    /// Returns whether our contents matches a search term.
    pub fn matches_search(&self, term: &wstr, typ: SearchType, case_sensitive: bool) -> bool {
        // Note that 'term' has already been lowercased when constructing the
        // search object if we're doing a case insensitive search.
        let content_to_match = if case_sensitive {
            Cow::Borrowed(&self.contents)
        } else {
            Cow::Owned(self.contents.to_lowercase())
        };

        match typ {
            SearchType::Exact => term == *content_to_match,
            SearchType::Contains => {
                find_subslice(term.as_slice(), content_to_match.as_slice()).is_some()
            }
            SearchType::Prefix => content_to_match.as_slice().starts_with(term.as_slice()),
            SearchType::LinePrefix => content_to_match
                .as_char_slice()
                .split(|&c| c == '\n')
                .any(|line| line.starts_with(term.as_char_slice())),
            SearchType::ContainsGlob => {
                let mut pat = parse_util_unescape_wildcards(term);
                if !pat.starts_with(ANY_STRING) {
                    pat.insert(0, ANY_STRING);
                }
                if !pat.ends_with(ANY_STRING) {
                    pat.push(ANY_STRING);
                }
                wildcard_match(content_to_match.as_ref(), &pat, false)
            }
            SearchType::PrefixGlob => {
                let mut pat = parse_util_unescape_wildcards(term);
                if !pat.ends_with(ANY_STRING) {
                    pat.push(ANY_STRING);
                }
                wildcard_match(content_to_match.as_ref(), &pat, false)
            }
            SearchType::ContainsSubsequence => subsequence_in_string(term, &content_to_match),
            SearchType::MatchEverything => true,
        }
    }

    /// Returns the timestamp for creating this history item.
    pub fn timestamp(&self) -> SystemTime {
        self.creation_timestamp
    }

    /// Returns whether this item should be persisted (written to disk).
    pub fn should_write_to_disk(&self) -> bool {
        self.persist_mode == PersistenceMode::Disk
    }

    /// Get the list of arguments which referred to files.
    /// This is used for autosuggestion hinting.
    pub fn get_required_paths(&self) -> &[WString] {
        &self.required_paths
    }

    /// Set the list of arguments which referred to files.
    /// This is used for autosuggestion hinting.
    pub fn set_required_paths(&mut self, paths: Vec<WString>) {
        self.required_paths = paths;
    }

    /// We can merge two items if they are the same command. We use the more recent timestamp, more
    /// recent identifier, and the longer list of required paths.
    fn merge(&mut self, item: &HistoryItem) -> bool {
        // We can only merge items if they agree on their text and persistence mode.
        if self.contents != item.contents || self.persist_mode != item.persist_mode {
            return false;
        }

        // Ok, merge this item.
        self.creation_timestamp = self.creation_timestamp.max(item.creation_timestamp);
        if self.required_paths.len() < item.required_paths.len() {
            self.required_paths = item.required_paths.clone();
        }
        if self.identifier < item.identifier {
            self.identifier = item.identifier;
        }
        true
    }
}

static HISTORIES: Mutex<BTreeMap<WString, Arc<History>>> = Mutex::new(BTreeMap::new());

struct HistoryImpl {
    /// The name of this list. Used for picking a suitable filename and for switching modes.
    name: WString,
    /// New items. Note that these are NOT discarded on save. We need to keep these around so we can
    /// distinguish between items in our history and items in the history of other shells that were
    /// started after we were started.
    new_items: Vec<HistoryItem>,
    /// The index of the first new item that we have not yet written.
    first_unwritten_new_item_index: usize, // 0
    /// Whether we have a pending item. If so, the most recently added item is ignored by
    /// item_at_index.
    has_pending_item: bool, // false
    /// Whether we should disable saving to the file for a time.
    disable_automatic_save_counter: u32, // 0
    /// Deleted item contents.
    /// Boolean describes if it should be deleted only in this session or in all
    /// (used in deduplication).
    deleted_items: HashMap<WString, bool>,
    /// The buffer containing the history file contents.
    file_contents: Option<HistoryFileContents>,
    /// The file ID of the history file.
    history_file_id: FileId, // INVALID_FILE_ID
    /// The boundary timestamp distinguishes old items from new items. Items whose timestamps are <=
    /// the boundary are considered "old". Items whose timestamps are > the boundary are new, and are
    /// ignored by this instance (unless they came from this instance). The timestamp may be adjusted
    /// by incorporate_external_changes().
    boundary_timestamp: SystemTime,
    /// The most recent "unique" identifier for a history item.
    last_identifier: HistoryIdentifier, // 0
    /// How many items we add until the next vacuum. Initially a random value.
    countdown_to_vacuum: Option<usize>,
    /// Whether we've loaded old items.
    loaded_old: bool, // false
    /// List of old items, as offsets into out mmap data.
    old_item_offsets: Vec<usize>,
}

/// If set, we gave up on file locking because it took too long.
/// Note this is shared among all history instances.
static ABANDONED_LOCKING: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

impl HistoryImpl {
    /// Add a new history item to the end. If `pending` is set, the item will not be returned by
    /// `item_at_index()` until a call to `resolve_pending()`. Pending items are tracked with an
    /// offset into the array of new items, so adding a non-pending item has the effect of resolving
    /// all pending items.
    fn add(
        &mut self,
        item: HistoryItem,
        pending: bool, /*=false*/
        do_save: bool, /*=true*/
    ) {
        // We use empty items as sentinels to indicate the end of history.
        // Do not allow them to be added (#6032).
        if item.contents.is_empty() {
            return;
        }

        // Try merging with the last item.
        if let Some(last) = self.new_items.last_mut() {
            if last.merge(&item) {
                // We merged, so we don't have to add anything. Maybe this item was pending, but it just got
                // merged with an item that is not pending, so pending just becomes false.
                self.has_pending_item = false;
                return;
            }
        }

        // We have to add a new item.
        self.new_items.push(item);
        self.has_pending_item = pending;
        if do_save {
            self.save_unless_disabled();
        }
    }

    /// Internal function.
    fn clear_file_state(&mut self) {
        // Erase everything we know about our file.
        self.file_contents = None;
        self.loaded_old = false;
        self.old_item_offsets.clear();
    }

    /// Returns a timestamp for new items - see the implementation for a subtlety.
    fn timestamp_now(&self) -> SystemTime {
        let mut when = SystemTime::now();
        // Big hack: do not allow timestamps equal to our boundary date. This is because we include
        // items whose timestamps are equal to our boundary when reading old history, so we can catch
        // "just closed" items. But this means that we may interpret our own items, that we just wrote,
        // as old items, if we wrote them in the same second as our birthdate.
        if when.duration_since(UNIX_EPOCH).ok().map(|d| d.as_secs())
            == self
                .boundary_timestamp
                .duration_since(UNIX_EPOCH)
                .ok()
                .map(|d| d.as_secs())
        {
            when += Duration::from_secs(1);
        }
        when
    }

    /// Returns a new item identifier, incrementing our counter.
    fn next_identifier(&mut self) -> HistoryIdentifier {
        self.last_identifier += 1;
        self.last_identifier
    }

    /// Figure out the offsets of our file contents.
    fn populate_from_file_contents(&mut self) {
        self.old_item_offsets.clear();
        if let Some(file_contents) = &self.file_contents {
            let mut cursor = 0;
            while let Some(offset) =
                file_contents.offset_of_next_item(&mut cursor, Some(self.boundary_timestamp))
            {
                // Remember this item.
                self.old_item_offsets.push(offset);
            }
        }

        FLOGF!(history, "Loaded %lu old items", self.old_item_offsets.len());
    }

    /// Loads old items if necessary.
    fn load_old_if_needed(&mut self) {
        if self.loaded_old {
            return;
        }
        self.loaded_old = true;

        let _profiler = TimeProfiler::new("load_old");
        if let Some(filename) = history_filename(&self.name, L!("")) {
            let Ok(mut file) = wopen_cloexec(&filename, OFlag::O_RDONLY, Mode::empty()) else {
                return;
            };

            // Take a read lock to guard against someone else appending. This is released after
            // getting the file's length. We will read the file after releasing the lock, but that's
            // not a problem, because we never modify already written data. In short, the purpose of
            // this lock is to ensure we don't see the file size change mid-update.
            //
            // We may fail to lock (e.g. on lockless NFS - see issue #685. In that case, we proceed
            // as if it did not fail. The risk is that we may get an incomplete history item; this
            // is unlikely because we only treat an item as valid if it has a terminating newline.
            let locked = unsafe { Self::maybe_lock_file(&mut file, LOCK_SH) };
            self.file_contents = HistoryFileContents::create(&mut file);
            self.history_file_id = if self.file_contents.is_some() {
                file_id_for_file(&file)
            } else {
                INVALID_FILE_ID
            };
            if locked {
                unsafe {
                    Self::unlock_file(&mut file);
                }
            }

            let _profiler = TimeProfiler::new("populate_from_file_contents");
            self.populate_from_file_contents();
        }
    }

    /// Deletes duplicates in new_items.
    fn compact_new_items(&mut self) {
        // Keep only the most recent items with the given contents.
        let mut seen = HashSet::new();
        for idx in (0..self.new_items.len()).rev() {
            let item = &self.new_items[idx];

            // Only compact persisted items.
            if !item.should_write_to_disk() {
                continue;
            }

            if !seen.insert(item.contents.to_owned()) {
                // This item was not inserted because it was already in the set, so delete the item at
                // this index.
                self.new_items.remove(idx);

                if idx < self.first_unwritten_new_item_index {
                    // Decrement first_unwritten_new_item_index if we are deleting a previously written
                    // item.
                    self.first_unwritten_new_item_index -= 1;
                }
            }
        }
    }

    /// Removes trailing ephemeral items.
    /// Ephemeral items have leading spaces, and can only be retrieved immediately; adding any item
    /// removes them.
    fn remove_ephemeral_items(&mut self) {
        while matches!(
            self.new_items.last(),
            Some(&HistoryItem {
                persist_mode: PersistenceMode::Ephemeral,
                ..
            })
        ) {
            self.new_items.pop();
        }

        self.first_unwritten_new_item_index =
            usize::min(self.first_unwritten_new_item_index, self.new_items.len());
    }

    /// Given an existing history file, write a new history file to `dst`.
    /// Returns false on error, true on success
    fn rewrite_to_temporary_file(&self, existing_file: Option<&mut File>, dst: &mut File) -> bool {
        // We are reading FROM existing_file and writing TO dst

        // Make an LRU cache to save only the last N elements.
        let mut lru = LruCache::new(HISTORY_SAVE_MAX);

        // Read in existing items (which may have changed out from underneath us, so don't trust our
        // old file contents).
        if let Some(existing_file) = existing_file {
            if let Some(local_file) = HistoryFileContents::create(existing_file) {
                let mut cursor = 0;
                while let Some(offset) = local_file.offset_of_next_item(&mut cursor, None) {
                    // Try decoding an old item.
                    let Some(old_item) = local_file.decode_item(offset) else {
                        continue;
                    };

                    // If old item is newer than session always erase if in deleted.
                    if old_item.timestamp() > self.boundary_timestamp {
                        if old_item.is_empty() || self.deleted_items.contains_key(old_item.str()) {
                            continue;
                        }
                        lru.add_item(old_item);
                    } else {
                        // If old item is older and in deleted items don't erase if added by
                        // clear_session.
                        if old_item.is_empty()
                            || self.deleted_items.get(old_item.str()) == Some(&false)
                        {
                            continue;
                        }
                        // Add this old item.
                        lru.add_item(old_item);
                    }
                }
            }
        }

        // Insert any unwritten new items
        for item in self
            .new_items
            .iter()
            .skip(self.first_unwritten_new_item_index)
        {
            if item.should_write_to_disk() {
                lru.add_item(item.clone());
            }
        }

        // Stable-sort our items by timestamp
        // This is because we may have read "old" items with a later timestamp than our "new" items
        // This is the essential step that roughly orders items by history
        let mut items: Vec<_> = lru.into_iter().map(|(_key, item)| item).collect();
        items.sort_by_key(HistoryItem::timestamp);

        // Write them out.
        let mut err = None;
        let mut buffer = Vec::with_capacity(HISTORY_OUTPUT_BUFFER_SIZE + 128);
        for item in items {
            append_history_item_to_buffer(&item, &mut buffer);
            if buffer.len() >= HISTORY_OUTPUT_BUFFER_SIZE {
                if let Err(e) = drain_buffer_into_file_no_flush(&mut buffer, dst) {
                    err = Some(e);
                    break;
                }
            }
        }
        if err.is_none() {
            if let Err(e) = drain_buffer_into_file_and_flush(&mut buffer, dst) {
                err = Some(e);
            }
        }
        if let Some(err) = err {
            FLOG!(
                history_file,
                "Error writing to temporary history file:",
                err
            );

            false
        } else {
            true
        }
    }

    /// Saves history by rewriting the file.
    fn save_internal_via_rewrite(&mut self) {
        FLOGF!(
            history,
            "Saving %lu items via rewrite",
            self.new_items.len() - self.first_unwritten_new_item_index
        );

        // We want to rewrite the file, while holding the lock for as briefly as possible
        // To do this, we speculatively write a file, and then lock and see if our original file changed
        // Repeat until we succeed or give up
        let Some(possibly_indirect_target_name) = history_filename(&self.name, L!("")) else {
            return;
        };
        let Some(tmp_name_template) = history_filename(&self.name, L!(".XXXXXX")) else {
            return;
        };

        // If the history file is a symlink, we want to rewrite the real file so long as we can find it.
        let target_name =
            wrealpath(&possibly_indirect_target_name).unwrap_or(possibly_indirect_target_name);

        // Make our temporary file
        let Some((mut tmp_file, tmp_name)) = create_temporary_file(&tmp_name_template) else {
            return;
        };
        let mut done = false;
        for _i in 0..MAX_SAVE_TRIES {
            if done {
                break;
            }

            let target_file_before = wopen_cloexec(
                &target_name,
                OFlag::O_RDONLY | OFlag::O_CREAT,
                HISTORY_FILE_MODE,
            );
            if let Err(err) = target_file_before {
                FLOG!(history_file, "Error opening history file:", err);
            }

            let orig_file_id = target_file_before
                .as_ref()
                .map(file_id_for_file)
                .unwrap_or(INVALID_FILE_ID);

            // Open any target file, but do not lock it right away
            if !self.rewrite_to_temporary_file(target_file_before.ok().as_mut(), &mut tmp_file) {
                // Failed to write, no good
                break;
            }

            // The crux! We rewrote the history file; see if the history file changed while we
            // were rewriting it. Make an effort to take the lock before checking, to avoid racing.
            // If the open fails, then proceed; this may be because there is no current history
            let mut new_file_id = INVALID_FILE_ID;

            let mut target_file_after = wopen_cloexec(&target_name, OFlag::O_RDONLY, Mode::empty());
            if let Ok(target_file_after) = target_file_after.as_mut() {
                // critical to take the lock before checking file IDs,
                // and hold it until after we are done replacing.
                // Also critical to check the file at the path, NOT based on our fd.
                // It's only OK to replace the file while holding the lock.
                // Note any lock is released when target_file_after is closed.
                unsafe {
                    Self::maybe_lock_file(target_file_after, LOCK_EX);
                }
                new_file_id = match file_id_for_path_or_error(&target_name) {
                    Ok(file_id) => file_id,
                    Err(err) => {
                        if err.kind() != std::io::ErrorKind::NotFound {
                            FLOG!(history_file, "Error re-opening history file:", err);
                        }
                        INVALID_FILE_ID
                    }
                }
            }

            let can_replace_file = new_file_id == orig_file_id || new_file_id == INVALID_FILE_ID;
            if !can_replace_file {
                // The file has changed, so we're going to re-read it
                // Truncate our tmp_file so we can reuse it
                if let Err(err) = tmp_file.set_len(0) {
                    FLOG!(
                        history_file,
                        "Error when truncating temporary history file:",
                        err
                    );
                }
                if let Err(err) = tmp_file.seek(SeekFrom::Start(0)) {
                    FLOG!(
                        history_file,
                        "Error resetting cursor in temporary history file:",
                        err
                    );
                }
            } else {
                // The file is unchanged, or the new file doesn't exist or we can't read it
                // We also attempted to take the lock, so we feel confident in replacing it

                // Ensure we maintain the ownership and permissions of the original (#2355). If the
                // stat fails, we assume (hope) our default permissions are correct. This
                // corresponds to e.g. someone running sudo -E as the very first command. If they
                // did, it would be tricky to set the permissions correctly. (bash doesn't get this
                // case right either).
                if let Ok(target_file_after) = target_file_after.as_ref() {
                    if let Ok(md) = target_file_after.metadata() {
                        // TODO(MSRV): Consider replacing with std::os::unix::fs::fchown when MSRV >= 1.73
                        if unsafe { fchown(tmp_file.as_raw_fd(), md.uid(), md.gid()) } == -1 {
                            FLOG!(
                                history_file,
                                "Error when changing ownership of history file:",
                                errno::errno()
                            );
                        }
                        if let Err(e) = tmp_file.set_permissions(md.permissions()) {
                            FLOG!(history_file, "Error when changing mode of history file:", e);
                        }
                    }
                }

                // Slide it into place
                if wrename(&tmp_name, &target_name) == -1 {
                    FLOG!(
                        error,
                        wgettext_fmt!(
                            "Error when renaming history file: %s",
                            errno::errno().to_string()
                        )
                    );
                }

                // We did it
                done = true;
            }
        }

        // Ensure we never leave the old file around
        let _ = wunlink(&tmp_name);

        if done {
            // We've saved everything, so we have no more unsaved items.
            self.first_unwritten_new_item_index = self.new_items.len();

            // We deleted our deleted items.
            self.deleted_items.clear();

            // Our history has been written to the file, so clear our state so we can re-reference the
            // file.
            self.clear_file_state();
        }
    }

    /// Saves history by appending to the file.
    fn save_internal_via_appending(&mut self) -> std::io::Result<()> {
        FLOGF!(
            history,
            "Saving %lu items via appending",
            self.new_items.len() - self.first_unwritten_new_item_index
        );
        // No deleting allowed.
        assert!(self.deleted_items.is_empty());

        // If the file is different (someone vacuumed it) then we need to update our mmap.
        let mut file_changed = false;

        // Get the path to the real history file.
        let Some(history_path) = history_filename(&self.name, L!("")) else {
            // No history should be saved if fish_history is set to the empty string,
            // so nothing needs to be done here.
            return Ok(());
        };

        // We are going to open the file, lock it, append to it, and then close it
        // After locking it, we need to stat the file at the path; if there is a new file there, it
        // means the file was replaced and we have to try again.
        // Limit our max tries so we don't do this forever.
        let mut num_attempts = 0;
        let mut history_file = loop {
            if num_attempts == MAX_SAVE_TRIES {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "Number of unsuccessful attempts to open history file exceeds maximum.",
                ));
            }
            // Return immediately if an error occurs here.
            let mut file = wopen_cloexec(
                &history_path,
                OFlag::O_WRONLY | OFlag::O_APPEND,
                Mode::empty(),
            )?;

            // Exclusive lock on the entire file. This is released when we close the file (below). This
            // may fail on (e.g.) lockless NFS. If so, proceed as if it did not fail; the risk is that
            // we may get interleaved history items, which is considered better than no history, or
            // forcing everything through the slow copy-move mode. We try to minimize this possibility
            // by writing with O_APPEND.
            unsafe {
                Self::maybe_lock_file(&mut file, LOCK_EX);
            }
            let file_id = file_id_for_file(&file);
            if file_id_for_path(&history_path) == file_id {
                // File IDs match, so the file we opened is still at that path
                // We're going to use this fd
                if file_id != self.history_file_id {
                    file_changed = true;
                }
                break file;
            }
            num_attempts += 1;
        };

        // We (hopefully successfully) took the exclusive lock. Append to the file.
        // Note that this is sketchy for a few reasons:
        //   - Another shell may have appended its own items with a later timestamp, so our file may
        // no longer be sorted by timestamp.
        //   - Another shell may have appended the same items, so our file may now contain
        // duplicates.
        //
        // We cannot modify any previous parts of our file, because other instances may be reading
        // those portions. We can only append.
        //
        // Originally we always rewrote the file on saving, which avoided both of these problems.
        // However, appending allows us to save history after every command, which is nice!
        //
        // Periodically we "clean up" the file by rewriting it, so that most of the time it doesn't
        // have duplicates, although we don't yet sort by timestamp (the timestamp isn't really used
        // for much anyways).

        // So far so good. Write all items at or after first_unwritten_new_item_index. Note that we
        // write even a pending item - pending items are ignored by history within the command
        // itself, but should still be written to the file.
        // TODO: consider filling the buffer ahead of time, so we can just lock, splat, and unlock?
        let mut res = Ok(());
        // Use a small buffer size for appending, we usually only have 1 item
        let mut buffer = Vec::new();
        while self.first_unwritten_new_item_index < self.new_items.len() {
            let item = &self.new_items[self.first_unwritten_new_item_index];
            if item.should_write_to_disk() {
                append_history_item_to_buffer(item, &mut buffer);
                // Ensure that each item is written individually and makes it to storage.
                // This is done to keep `self.first_unwritten_new_item_index` consistent with
                // the file system.
                // Flushing and syncing each iteration adds overhead,
                // but hopefully there are not that many items to write when appending.
                res = drain_buffer_into_file_and_flush(&mut buffer, &mut history_file);
                if res.is_err() {
                    break;
                }
            }
            // We wrote or skipped this item, hooray.
            self.first_unwritten_new_item_index += 1;
        }

        // Since we just modified the file, update our history_file_id to match its current state
        // Otherwise we'll think the file has been changed by someone else the next time we go to
        // write.
        // We don't update the mapping since we only appended to the file, and everything we
        // appended remains in our new_items
        self.history_file_id = file_id_for_file(&history_file);

        drop(history_file);

        // If someone has replaced the file, forget our file state.
        if file_changed {
            self.clear_file_state();
        }

        res
    }

    /// Saves history.
    fn save(&mut self, vacuum: bool) {
        // Nothing to do if there's no new items.
        if self.first_unwritten_new_item_index >= self.new_items.len()
            && self.deleted_items.is_empty()
        {
            return;
        }

        if history_filename(&self.name, L!("")).is_none() {
            // We're in the "incognito" mode. Pretend we've saved the history.
            self.first_unwritten_new_item_index = self.new_items.len();
            self.deleted_items.clear();
            self.clear_file_state();
        }

        // Compact our new items so we don't have duplicates.
        self.compact_new_items();

        // Try saving. If we have items to delete, we have to rewrite the file. If we do not, we can
        // append to it.
        let mut ok = false;
        if !vacuum && self.deleted_items.is_empty() {
            // Try doing a fast append.
            if let Err(e) = self.save_internal_via_appending() {
                FLOG!(history, "Appending failed", e);
            } else {
                ok = true;
            }
        }
        if !ok {
            // We did not or could not append; rewrite the file ("vacuum" it).
            self.save_internal_via_rewrite();
        }
    }

    /// Saves history unless doing so is disabled.
    fn save_unless_disabled(&mut self) {
        // Respect disable_automatic_save_counter.
        if self.disable_automatic_save_counter > 0 {
            return;
        }

        // We may or may not vacuum. We try to vacuum every kVacuumFrequency items, but start the
        // countdown at a random number so that even if the user never runs more than 25 commands, we'll
        // eventually vacuum.  If countdown_to_vacuum is None, it means we haven't yet picked a value for
        // the counter.
        let countdown_to_vacuum = self
            .countdown_to_vacuum
            .get_or_insert_with(|| get_rng().gen_range(0..VACUUM_FREQUENCY));

        // Determine if we're going to vacuum.
        let mut vacuum = false;
        if *countdown_to_vacuum == 0 {
            *countdown_to_vacuum = VACUUM_FREQUENCY;
            vacuum = true;
        }

        // Update our countdown.
        assert!(*countdown_to_vacuum > 0);
        *countdown_to_vacuum -= 1;

        // This might be a good candidate for moving to a background thread.
        let _profiler = TimeProfiler::new(if vacuum {
            "save vacuum"
        } else {
            "save no vacuum"
        });
        self.save(vacuum);
    }

    fn new(name: WString) -> Self {
        Self {
            name,
            new_items: vec![],
            first_unwritten_new_item_index: 0,
            has_pending_item: false,
            disable_automatic_save_counter: 0,
            deleted_items: HashMap::new(),
            file_contents: None,
            history_file_id: INVALID_FILE_ID,
            boundary_timestamp: SystemTime::now(),
            last_identifier: 0,
            countdown_to_vacuum: None,
            loaded_old: false,
            old_item_offsets: Vec::new(),
        }
    }

    /// Returns whether this is using the default name.
    fn is_default(&self) -> bool {
        self.name == DFLT_FISH_HISTORY_SESSION_ID
    }

    /// Determines whether the history is empty. Unfortunately this cannot be const, since it may
    /// require populating the history.
    fn is_empty(&mut self) -> bool {
        // If we have new items, we're not empty.
        if !self.new_items.is_empty() {
            return false;
        }

        if self.loaded_old {
            // If we've loaded old items, see if we have any offsets.
            self.old_item_offsets.is_empty()
        } else {
            // If we have not loaded old items, don't actually load them (which may be expensive); just
            // stat the file and see if it exists and is nonempty.
            let Some(where_) = history_filename(&self.name, L!("")) else {
                return true;
            };

            if let Ok(md) = wstat(&where_) {
                // We're empty if the file is empty.
                md.len() == 0
            } else {
                // Access failed, assume missing.
                true
            }
        }
    }

    /// Remove a history item.
    fn remove(&mut self, str_to_remove: &wstr) {
        // Add to our list of deleted items.
        self.deleted_items.insert(str_to_remove.to_owned(), false);

        for idx in (0..self.new_items.len()).rev() {
            let matched = self.new_items[idx].str() == str_to_remove;
            if matched {
                self.new_items.remove(idx);
                // If this index is before our first_unwritten_new_item_index, then subtract one from
                // that index so it stays pointing at the same item. If it is equal to or larger, then
                // we have not yet written this item, so we don't have to adjust the index.
                if idx < self.first_unwritten_new_item_index {
                    self.first_unwritten_new_item_index -= 1;
                }
            }
        }
        assert!(self.first_unwritten_new_item_index <= self.new_items.len());
    }

    /// Resolves any pending history items, so that they may be returned in history searches.
    fn resolve_pending(&mut self) {
        self.has_pending_item = false;
    }

    /// Enable / disable automatic saving. Main thread only!
    fn disable_automatic_saving(&mut self) {
        self.disable_automatic_save_counter += 1;
        assert!(self.disable_automatic_save_counter != 0); // overflow!
    }

    fn enable_automatic_saving(&mut self) {
        assert!(self.disable_automatic_save_counter > 0); // negative overflow!
        self.disable_automatic_save_counter -= 1;
    }

    /// Irreversibly clears history.
    fn clear(&mut self) {
        self.new_items.clear();
        self.deleted_items.clear();
        self.first_unwritten_new_item_index = 0;
        self.old_item_offsets.clear();
        if let Some(filename) = history_filename(&self.name, L!("")) {
            wunlink(&filename);
        }
        self.clear_file_state();
    }

    /// Clears only session.
    fn clear_session(&mut self) {
        for item in &self.new_items {
            self.deleted_items.insert(item.str().to_owned(), true);
        }

        self.new_items.clear();
        self.first_unwritten_new_item_index = 0;
    }

    /// Populates from older location (in config path, rather than data path).
    /// This is accomplished by clearing ourselves, and copying the contents of the old history
    /// file to the new history file.
    /// The new contents will automatically be re-mapped later.
    fn populate_from_config_path(&mut self) {
        let Some(new_file) = history_filename(&self.name, L!("")) else {
            return;
        };

        let Some(mut old_file) = path_get_config() else {
            return;
        };

        old_file.push('/');
        old_file.push_utfstr(&self.name);
        old_file.push_str("_history");

        let Ok(mut src_file) = wopen_cloexec(&old_file, OFlag::O_RDONLY, Mode::empty()) else {
            return;
        };

        // Clear must come after we've retrieved the new_file name, and before we open
        // destination file descriptor, since it destroys the name and the file.
        self.clear();

        let mut dst_file = match wopen_cloexec(
            &new_file,
            OFlag::O_WRONLY | OFlag::O_CREAT,
            HISTORY_FILE_MODE,
        ) {
            Ok(file) => file,
            Err(err) => {
                FLOG!(history_file, "Error when writing history file:", err);
                return;
            }
        };

        let mut buf = [0; libc::BUFSIZ as usize];
        while let Ok(n) = src_file.read(&mut buf) {
            if n == 0 {
                break;
            }

            if let Err(err) = dst_file.write_all(&buf[..n]) {
                FLOG!(history_file, "Error when writing history file:", err);
                break;
            }
        }
    }

    /// Import a bash command history file. Bash's history format is very simple: just lines with
    /// `#`s for comments. Ignore a few commands that are bash-specific. It makes no attempt to
    /// handle multiline commands. We can't actually parse bash syntax and the bash history file
    /// does not unambiguously encode multiline commands.
    fn populate_from_bash<R: BufRead>(&mut self, contents: R) {
        // Process the entire history file until EOF is observed.
        // Pretend all items were created at this time.
        let when = self.timestamp_now();
        for line in contents.split(b'\n') {
            let Ok(line) = line else {
                break;
            };
            let wide_line = trim(str2wcstring(&line), None);
            // Add this line if it doesn't contain anything we know we can't handle.
            if should_import_bash_history_line(&wide_line) {
                self.add(
                    HistoryItem::new(wide_line, when, 0, PersistenceMode::Disk),
                    /*pending=*/ false,
                    /*do_save=*/ false,
                );
            }
        }
        self.save_unless_disabled();
    }

    /// Incorporates the history of other shells into this history.
    fn incorporate_external_changes(&mut self) {
        // To incorporate new items, we simply update our timestamp to now, so that items from previous
        // instances get added. We then clear the file state so that we remap the file. Note that this
        // is somewhat expensive because we will be going back over old items. An optimization would be
        // to preserve old_item_offsets so that they don't have to be recomputed. (However, then items
        // *deleted* in other instances would not show up here).
        let new_timestamp = SystemTime::now();

        // If for some reason the clock went backwards, we don't want to start dropping items; therefore
        // we only do work if time has progressed. This also makes multiple calls cheap.
        if new_timestamp > self.boundary_timestamp {
            self.boundary_timestamp = new_timestamp;
            self.clear_file_state();

            // We also need to erase new items, since we go through those first, and that means we
            // will not properly interleave them with items from other instances.
            // We'll pick them up from the file (#2312)
            // TODO: this will drop items that had no_persist set, how can we avoid that while still
            // properly interleaving?
            self.save(false);
            self.new_items.clear();
            self.first_unwritten_new_item_index = 0;
        }
    }

    /// Gets all the history into a list. This is intended for the $history environment variable.
    /// This may be long!
    fn get_history(&mut self) -> Vec<WString> {
        let mut result = vec![];
        // If we have a pending item, we skip the first encountered (i.e. last) new item.
        let mut next_is_pending = self.has_pending_item;
        let mut seen = HashSet::new();

        // Append new items.
        for item in self.new_items.iter().rev() {
            // Skip a pending item if we have one.
            if next_is_pending {
                next_is_pending = false;
                continue;
            }

            if seen.insert(item.str().to_owned()) {
                result.push(item.str().to_owned())
            }
        }

        // Append old items.
        self.load_old_if_needed();
        for &offset in self.old_item_offsets.iter().rev() {
            let Some(item) = self.file_contents.as_ref().unwrap().decode_item(offset) else {
                continue;
            };
            if seen.insert(item.str().to_owned()) {
                result.push(item.str().to_owned());
            }
        }
        result
    }

    /// Let indexes be a list of one-based indexes into the history, matching the interpretation of
    /// `$history`. That is, `$history[1]` is the most recently executed command. Values less than one
    /// are skipped. Return a mapping from index to history item text.
    fn items_at_indexes(
        &mut self,
        indexes: impl IntoIterator<Item = usize>,
    ) -> HashMap<usize, WString> {
        let mut result = HashMap::new();
        for idx in indexes {
            // If this is the first time the index is encountered, we have to go fetch the item.
            #[allow(clippy::map_entry)] // looks worse
            if !result.contains_key(&idx) {
                // New key.
                let contents = match self.item_at_index(idx) {
                    None => WString::new(),
                    Some(Cow::Borrowed(HistoryItem { contents, .. })) => contents.clone(),
                    Some(Cow::Owned(HistoryItem { contents, .. })) => contents,
                };
                result.insert(idx, contents);
            }
        }
        result
    }

    /// Sets the valid file paths for the history item with the given identifier.
    fn set_valid_file_paths(&mut self, valid_file_paths: Vec<WString>, ident: HistoryIdentifier) {
        // 0 identifier is used to mean "not necessary".
        if ident == 0 {
            return;
        }

        // Look for an item with the given identifier. It is likely to be at the end of new_items.
        for item in self.new_items.iter_mut().rev() {
            if item.identifier == ident {
                // found it
                item.required_paths = valid_file_paths;
                break;
            }
        }
    }

    /// Return the specified history at the specified index. 0 is the index of the current
    /// commandline. (So the most recent item is at index 1.)
    fn item_at_index(&mut self, mut idx: usize) -> Option<Cow<HistoryItem>> {
        // 0 is considered an invalid index.
        if idx == 0 {
            return None;
        }
        idx -= 1;

        // Determine how many "resolved" (non-pending) items we have. We can have at most one pending
        // item, and it's always the last one.
        let mut resolved_new_item_count = self.new_items.len();
        if self.has_pending_item && resolved_new_item_count > 0 {
            resolved_new_item_count -= 1;
        }

        // idx == 0 corresponds to the last resolved item.
        if idx < resolved_new_item_count {
            return Some(Cow::Borrowed(
                &self.new_items[resolved_new_item_count - idx - 1],
            ));
        }

        // Now look in our old items.
        idx -= resolved_new_item_count;
        self.load_old_if_needed();
        if let Some(file_contents) = &self.file_contents {
            let old_item_count = self.old_item_offsets.len();
            if idx < old_item_count {
                // idx == 0 corresponds to last item in old_item_offsets.
                let offset = self.old_item_offsets[old_item_count - idx - 1];
                return file_contents.decode_item(offset).map(Cow::Owned);
            }
        }

        // Index past the valid range, so return None.
        return None;
    }

    /// Return the number of history entries.
    fn size(&mut self) -> usize {
        let mut new_item_count = self.new_items.len();
        if self.has_pending_item && new_item_count > 0 {
            new_item_count -= 1;
        }
        self.load_old_if_needed();
        let old_item_count = self.old_item_offsets.len();
        return new_item_count + old_item_count;
    }

    /// Maybe lock a history file.
    /// Returns `true` if successful, `false` if locking was skipped.
    ///
    /// # Safety
    ///
    /// `fd` and `lock_type` must be valid arguments to `flock(2)`.
    unsafe fn maybe_lock_file(file: &mut File, lock_type: libc::c_int) -> bool {
        assert!(lock_type & LOCK_UN == 0, "Do not use lock_file to unlock");

        // Don't lock if it took too long before, if we are simulating a failing lock, or if our history
        // is on a remote filesystem.
        if ABANDONED_LOCKING.load() {
            return false;
        }
        if CHAOS_MODE.load() {
            return false;
        }
        if path_get_data_remoteness() == DirRemoteness::remote {
            return false;
        }

        let start_time = SystemTime::now();
        let retval = unsafe { flock(file.as_raw_fd(), lock_type) };
        if let Ok(duration) = start_time.elapsed() {
            if duration > Duration::from_millis(250) {
                FLOG!(
                    warning,
                    wgettext_fmt!(
                        "Locking the history file took too long (%.3f seconds).",
                        duration.as_secs_f64()
                    )
                );
                ABANDONED_LOCKING.store(true);
            }
        }
        retval != -1
    }

    /// Unlock a history file.
    ///
    /// # Safety
    ///
    /// `fd` must be a valid argument to `flock(2)` with `LOCK_UN`.
    unsafe fn unlock_file(file: &mut File) {
        libc::flock(file.as_raw_fd(), LOCK_UN);
    }
}

// Returns the fd of an opened temporary file, or None on failure.
fn create_temporary_file(name_template: &wstr) -> Option<(File, WString)> {
    for _attempt in 0..10 {
        let narrow_str = wcs2zstring(name_template);
        if let Ok((fd, narrow_str)) = fish_mkstemp_cloexec(narrow_str) {
            return Some((fd, str2wcstring(narrow_str.to_bytes())));
        }
    }
    None
}

fn string_could_be_path(potential_path: &wstr) -> bool {
    // Assume that things with leading dashes aren't paths.
    return !(potential_path.is_empty() || potential_path.starts_with('-'));
}

/// Perform a search of `hist` for `search_string`. Invoke a function `func` for each match. If
/// `func` returns [`ControlFlow::Break`], stop the search.
fn do_1_history_search(
    hist: Arc<History>,
    search_type: SearchType,
    search_string: WString,
    case_sensitive: bool,
    mut func: impl FnMut(&HistoryItem) -> ControlFlow<(), ()>,
    cancel_check: &CancelChecker,
) {
    let mut searcher = HistorySearch::new_with(
        hist,
        search_string,
        search_type,
        if case_sensitive {
            SearchFlags::empty()
        } else {
            SearchFlags::IGNORE_CASE
        },
        0,
    );
    while !cancel_check() && searcher.go_to_next_match(SearchDirection::Backward) {
        if let ControlFlow::Break(()) = func(searcher.current_item()) {
            break;
        }
    }
}

/// Formats a single history record, including a trailing newline.
///
/// Returns nothing. The only possible failure involves formatting the timestamp. If that happens we
/// simply omit the timestamp from the output.
fn format_history_record(
    item: &HistoryItem,
    show_time_format: Option<&str>,
    null_terminate: bool,
) -> WString {
    let mut result = WString::new();
    let seconds = time_to_seconds(item.timestamp());
    // This warns for musl, but the warning is useless to us - there is nothing we can or should do.
    #[allow(deprecated)]
    let seconds = seconds as libc::time_t;
    let mut timestamp = MaybeUninit::uninit();
    if let Some(show_time_format) = show_time_format.and_then(|s| CString::new(s).ok()) {
        if !unsafe { libc::localtime_r(&seconds, timestamp.as_mut_ptr()).is_null() } {
            const max_tstamp_length: usize = 100;
            let mut timestamp_str = [0_u8; max_tstamp_length];
            use libc::strftime;
            if unsafe {
                strftime(
                    &mut timestamp_str[0] as *mut u8 as *mut libc::c_char,
                    max_tstamp_length,
                    show_time_format.as_ptr(),
                    timestamp.as_ptr(),
                )
            } != 0
            {
                result.push_utfstr(&cstr2wcstring(&timestamp_str[..]));
            }
        }
    }

    result.push_utfstr(item.str());
    result.push(if null_terminate { '\0' } else { '\n' });
    result
}

/// Decide whether we ought to import a bash history line into fish. This is a very crude heuristic.
fn should_import_bash_history_line(line: &wstr) -> bool {
    if line.is_empty() {
        return false;
    }

    // The following are Very naive tests!

    // Skip comments.
    if line.starts_with('#') {
        return false;
    }

    // Skip lines with backticks because we don't have that syntax,
    // Skip brace expansions and globs because they don't work like ours
    // Skip lines that end with a backslash. We do not handle multiline commands from bash history.
    if line.chars().any(|c| matches!(c, '`' | '{' | '*' | '\\')) {
        return false;
    }

    // Skip lines with [[...]] and ((...)) since we don't handle those constructs.
    // "<<" here is a proxy for heredocs (and herestrings).
    for seq in [L!("[["), L!("]]"), L!("(("), L!("))"), L!("<<")] {
        if find_subslice(seq, line.as_char_slice()).is_some() {
            return false;
        }
    }

    if ast::parse(line, ParseTreeFlags::empty(), None).errored() {
        return false;
    }

    // In doing this test do not allow incomplete strings. Hence the "false" argument.
    let mut errors = Vec::new();
    let _ = parse_util_detect_errors(line, Some(&mut errors), false);
    errors.is_empty()
}

pub struct History(Mutex<HistoryImpl>);

impl History {
    fn imp(&self) -> MutexGuard<HistoryImpl> {
        self.0.lock().unwrap()
    }

    /// Privately add an item. If pending, the item will not be returned by history searches until a
    /// call to resolve_pending. Any trailing ephemeral items are dropped.
    /// Exposed for testing.
    pub fn add(&self, item: HistoryItem, pending: bool /*=false*/) {
        self.imp().add(item, pending, true)
    }

    pub fn add_commandline(&self, s: WString) {
        let mut imp = self.imp();
        let when = imp.timestamp_now();
        let item = HistoryItem::new(s, when, 0, PersistenceMode::Disk);
        imp.add(item, false, true)
    }

    pub fn new(name: &wstr) -> Arc<Self> {
        Arc::new(Self(Mutex::new(HistoryImpl::new(name.to_owned()))))
    }

    /// Returns history with the given name, creating it if necessary.
    pub fn with_name(name: &wstr) -> Arc<Self> {
        let mut histories = HISTORIES.lock().unwrap();

        if let Some(hist) = histories.get(name) {
            Arc::clone(hist)
        } else {
            let hist = Self::new(name);
            histories.insert(name.to_owned(), Arc::clone(&hist));
            hist
        }
    }

    /// Returns whether this is using the default name.
    pub fn is_default(&self) -> bool {
        self.imp().is_default()
    }

    /// Determines whether the history is empty.
    pub fn is_empty(&self) -> bool {
        self.imp().is_empty()
    }

    /// Remove a history item.
    pub fn remove(&self, s: &wstr) {
        self.imp().remove(s)
    }

    /// Remove any trailing ephemeral items.
    pub fn remove_ephemeral_items(&self) {
        self.imp().remove_ephemeral_items()
    }

    /// Add a new pending history item to the end, and then begin file detection on the items to
    /// determine which arguments are paths. Arguments may be expanded (e.g. with PWD and variables)
    /// using the given `vars`. The item has the given `persist_mode`.
    pub fn add_pending_with_file_detection(
        self: Arc<Self>,
        s: &wstr,
        vars: &EnvStack,
        persist_mode: PersistenceMode, /*=disk*/
    ) {
        // We use empty items as sentinels to indicate the end of history.
        // Do not allow them to be added (#6032).
        if s.is_empty() {
            return;
        }

        // Find all arguments that look like they could be file paths.
        let mut needs_sync_write = false;
        let ast = ast::parse(s, ParseTreeFlags::empty(), None);

        let mut potential_paths = Vec::new();
        for node in ast.walk() {
            if let Kind::Argument(arg) = node.kind() {
                let potential_path = arg.source(s);
                if string_could_be_path(potential_path) {
                    potential_paths.push(potential_path.to_owned());
                }
            } else if let Kind::DecoratedStatement(stmt) = node.kind() {
                // Hack hack hack - if the command is likely to trigger an exit, then don't do
                // background file detection, because we won't be able to write it to our history file
                // before we exit.
                // Also skip it for 'echo'. This is because echo doesn't take file paths, but also
                // because the history file test wants to find the commands in the history file
                // immediately after running them, so it can't tolerate the asynchronous file detection.
                if stmt.decoration() == StatementDecoration::exec {
                    needs_sync_write = true;
                }

                let source = stmt.command.source(s);
                let command = unescape_string(source, UnescapeStringStyle::default());
                let command = command.as_deref().unwrap_or(source);
                if [L!("exit"), L!("reboot"), L!("restart"), L!("echo")].contains(&command) {
                    needs_sync_write = true;
                }
            }
        }

        // If we got a path, we'll perform file detection for autosuggestion hinting.
        let wants_file_detection = !potential_paths.is_empty() && !needs_sync_write;
        let mut imp = self.imp();

        // Make our history item.
        let when = imp.timestamp_now();
        let identifier = imp.next_identifier();
        let item = HistoryItem::new(s.to_owned(), when, identifier, persist_mode);
        let to_disk = persist_mode == PersistenceMode::Disk;

        if wants_file_detection {
            imp.disable_automatic_saving();

            // Add the item. Then check for which paths are valid on a background thread,
            // and unblock the item.
            // Don't hold the lock while we perform this file detection.
            imp.add(item, /*pending=*/ true, to_disk);
            drop(imp);
            let vars_snapshot = vars.snapshot();
            iothread_perform(move || {
                // Don't hold the lock while we perform this file detection.
                let validated_paths = expand_and_detect_paths(potential_paths, &vars_snapshot);
                let mut imp = self.imp();
                imp.set_valid_file_paths(validated_paths, identifier);
                imp.enable_automatic_saving();
                if to_disk {
                    imp.save_unless_disabled();
                }
            });
        } else {
            // Add the item.
            // If we think we're about to exit, save immediately, regardless of any disabling. This may
            // cause us to lose file hinting for some commands, but it beats losing history items.
            imp.add(item, /*pending=*/ true, to_disk);
            if to_disk && needs_sync_write {
                imp.save(false);
            }
        }
    }

    /// Resolves any pending history items, so that they may be returned in history searches.
    pub fn resolve_pending(&self) {
        self.imp().resolve_pending()
    }

    /// Saves history.
    pub fn save(&self) {
        self.imp().save(false)
    }

    /// Searches history.
    #[allow(clippy::too_many_arguments)]
    pub fn search(
        self: &Arc<Self>,
        search_type: SearchType,
        search_args: &[&wstr],
        show_time_format: Option<&str>,
        max_items: usize,
        case_sensitive: bool,
        null_terminate: bool,
        reverse: bool,
        cancel_check: &CancelChecker,
        streams: &mut IoStreams,
    ) -> bool {
        let mut remaining = max_items;
        let mut collected = Vec::new();
        let mut output_error = false;

        // The function we use to act on each item.
        let mut func = |item: &HistoryItem| {
            if remaining == 0 {
                return ControlFlow::Break(());
            }
            remaining -= 1;
            let formatted_record = format_history_record(item, show_time_format, null_terminate);
            if reverse {
                // We need to collect this for later.
                collected.push(formatted_record);
            } else {
                // We can output this immediately.
                if !streams.out.append(formatted_record) {
                    // This can happen if the user hit Ctrl-C to abort (maybe after the first page?).
                    output_error = true;
                    return ControlFlow::Break(());
                }
            }
            ControlFlow::Continue(())
        };

        if search_args.is_empty() {
            // The user had no search terms; just append everything.
            do_1_history_search(
                Arc::clone(self),
                SearchType::MatchEverything,
                WString::new(),
                false,
                &mut func,
                cancel_check,
            );
        } else {
            #[allow(clippy::unnecessary_to_owned)]
            for search_string in search_args.iter().copied() {
                if search_string.is_empty() {
                    streams
                        .err
                        .append(L!("Searching for the empty string isn't allowed"));
                    return false;
                }
                do_1_history_search(
                    Arc::clone(self),
                    search_type,
                    search_string.to_owned(),
                    case_sensitive,
                    &mut func,
                    cancel_check,
                );
            }
        }

        // Output any items we collected (which only happens in reverse).
        for item in collected.into_iter().rev() {
            if output_error {
                break;
            }

            if !streams.out.append(item) {
                // Don't force an error if output was aborted (typically via Ctrl-C/SIGINT); just don't
                // try writing any more.
                output_error = true;
            }
        }

        // We are intentionally not returning false in case of an output error, as the user aborting the
        // output early (the most common case) isn't a reason to exit w/ a non-zero status code.
        true
    }

    /// Irreversibly clears history.
    pub fn clear(&self) {
        self.imp().clear()
    }

    /// Irreversibly clears history for the current session.
    pub fn clear_session(&self) {
        self.imp().clear_session()
    }

    /// Populates from older location (in config path, rather than data path).
    pub fn populate_from_config_path(&self) {
        self.imp().populate_from_config_path()
    }

    /// Populates from a bash history file.
    pub fn populate_from_bash<R: BufRead>(&self, contents: R) {
        self.imp().populate_from_bash(contents)
    }

    /// Incorporates the history of other shells into this history.
    pub fn incorporate_external_changes(&self) {
        self.imp().incorporate_external_changes()
    }

    /// Gets all the history into a list. This is intended for the $history environment variable.
    /// This may be long!
    pub fn get_history(&self) -> Vec<WString> {
        self.imp().get_history()
    }

    /// Let indexes be a list of one-based indexes into the history, matching the interpretation of
    /// `$history`. That is, `$history[1]` is the most recently executed command.
    /// Returns a mapping from index to history item text.
    pub fn items_at_indexes(
        &self,
        indexes: impl IntoIterator<Item = usize>,
    ) -> HashMap<usize, WString> {
        self.imp().items_at_indexes(indexes)
    }

    /// Return the specified history at the specified index. 0 is the index of the current
    /// commandline. (So the most recent item is at index 1.)
    pub fn item_at_index(&self, idx: usize) -> Option<HistoryItem> {
        self.imp().item_at_index(idx).map(Cow::into_owned)
    }

    /// Return the number of history entries.
    pub fn size(&self) -> usize {
        self.imp().size()
    }
}

bitflags! {
    /// Flags for history searching.
    #[derive(Clone, Copy, Default)]
    pub struct SearchFlags: u32 {
        /// If set, ignore case.
        const IGNORE_CASE = 1 << 0;
        /// If set, do not deduplicate, which can help performance.
        const NO_DEDUP = 1 << 1;
    }
}

/// Support for searching a history backwards.
/// Note this does NOT de-duplicate; it is the caller's responsibility to do so.
pub struct HistorySearch {
    /// The history in which we are searching.
    history: Arc<History>,
    /// The original search term.
    orig_term: WString,
    /// The (possibly lowercased) search term.
    canon_term: WString,
    /// Our search type.
    search_type: SearchType, // history_search_type_t::contains
    /// Our flags.
    flags: SearchFlags, // 0
    /// The current history item.
    current_item: Option<HistoryItem>,
    /// Index of the current history item.
    current_index: usize, // 0
    /// If deduping, the items we've seen.
    deduper: HashSet<WString>,
}

impl HistorySearch {
    pub fn new(hist: Arc<History>, s: WString) -> Self {
        Self::new_with(hist, s, SearchType::Contains, SearchFlags::default(), 0)
    }
    pub fn new_with_type(hist: Arc<History>, s: WString, search_type: SearchType) -> Self {
        Self::new_with(hist, s, search_type, SearchFlags::default(), 0)
    }
    pub fn new_with_flags(hist: Arc<History>, s: WString, flags: SearchFlags) -> Self {
        Self::new_with(hist, s, SearchType::Contains, flags, 0)
    }
    /// Constructs a new history search.
    pub fn new_with(
        hist: Arc<History>,
        s: WString,
        search_type: SearchType,
        flags: SearchFlags,
        starting_index: usize,
    ) -> Self {
        let mut search = Self {
            history: hist,
            orig_term: s.clone(),
            canon_term: s,
            search_type,
            flags,
            current_item: None,
            current_index: starting_index,
            deduper: HashSet::new(),
        };

        if search.ignores_case() {
            search.canon_term = search.canon_term.to_lowercase();
        }

        search
    }

    /// Returns the original search term.
    pub fn original_term(&self) -> &wstr {
        &self.orig_term
    }

    pub fn prepare_to_search_after_deletion(&mut self) {
        assert!(self.current_index != 0);
        self.current_index -= 1;
        self.current_item = None;
    }

    /// Finds the next search result. Returns `true` if one was found.
    pub fn go_to_next_match(&mut self, direction: SearchDirection) -> bool {
        let invalid_index = match direction {
            SearchDirection::Backward => usize::MAX,
            SearchDirection::Forward => 0,
        };

        if self.current_index == invalid_index {
            return false;
        }

        let mut index = self.current_index;
        loop {
            // Backwards means increasing our index.
            match direction {
                SearchDirection::Backward => index += 1,
                SearchDirection::Forward => index -= 1,
            };

            if self.current_index == invalid_index {
                return false;
            }

            // We're done if it's empty or we cancelled.
            let Some(item) = self.history.item_at_index(index) else {
                self.current_index = match direction {
                    SearchDirection::Backward => self.history.size() + 1,
                    SearchDirection::Forward => 0,
                };
                self.current_item = None;
                return false;
            };

            // Look for an item that matches and (if deduping) that we haven't seen before.
            if !item.matches_search(&self.canon_term, self.search_type, !self.ignores_case()) {
                continue;
            }

            // Skip if deduplicating.
            if self.dedup() && !self.deduper.insert(item.str().to_owned()) {
                continue;
            }

            // This is our new item.
            self.current_item = Some(item);
            self.current_index = index;
            return true;
        }
    }

    /// Move current index so there is `value` matches in between new and old indexes
    pub fn search_forward(&mut self, value: usize) {
        while self.go_to_next_match(SearchDirection::Forward) && self.deduper.len() <= value {}
        self.deduper.clear();
    }

    /// Returns the current search result item.
    ///
    /// # Panics
    ///
    /// This function panics if there is no current item.
    pub fn current_item(&self) -> &HistoryItem {
        self.current_item.as_ref().expect("No current item")
    }

    /// Returns the current search result item contents.
    ///
    /// # Panics
    ///
    /// This function panics if there is no current item.
    pub fn current_string(&self) -> &wstr {
        self.current_item().str()
    }

    /// Returns the index of the current history item.
    pub fn current_index(&self) -> usize {
        self.current_index
    }

    /// Returns whether we are case insensitive.
    pub fn ignores_case(&self) -> bool {
        self.flags.contains(SearchFlags::IGNORE_CASE)
    }

    /// Returns whether we deduplicate items.
    fn dedup(&self) -> bool {
        !self.flags.contains(SearchFlags::NO_DEDUP)
    }
}

/// Saves the new history to disk.
pub fn save_all() {
    for hist in HISTORIES.lock().unwrap().values() {
        hist.save();
    }
}

/// Return the prefix for the files to be used for command and read history.
pub fn history_session_id(vars: &dyn Environment) -> WString {
    history_session_id_from_var(vars.get(L!("fish_history")))
}

pub fn history_session_id_from_var(history_name_var: Option<EnvVar>) -> WString {
    let Some(var) = history_name_var else {
        return DFLT_FISH_HISTORY_SESSION_ID.to_owned();
    };
    let session_id = var.as_string();
    if session_id.is_empty() || valid_var_name(&session_id) {
        session_id
    } else {
        FLOG!(
            error,
            wgettext_fmt!(
                "History session ID '%ls' is not a valid variable name. Falling back to `%ls`.",
                &session_id,
                DFLT_FISH_HISTORY_SESSION_ID
            ),
        );
        DFLT_FISH_HISTORY_SESSION_ID.to_owned()
    }
}

/// Given a list of proposed paths and a context, perform variable and home directory expansion,
/// and detect if the result expands to a value which is also the path to a file.
/// Wildcard expansions are suppressed - see implementation comments for why.
///
/// This is used for autosuggestion hinting. If we add an item to history, and one of its arguments
/// refers to a file, then we only want to suggest it if there is a valid file there.
/// This does disk I/O and may only be called in a background thread.
pub fn expand_and_detect_paths<P: IntoIterator<Item = WString>>(
    paths: P,
    vars: &dyn Environment,
) -> Vec<WString> {
    assert_is_background_thread();
    let working_directory = vars.get_pwd_slash();
    let ctx = OperationContext::background(vars, EXPANSION_LIMIT_BACKGROUND);
    let mut result = Vec::new();
    for path in paths {
        // Suppress cmdsubs since we are on a background thread and don't want to execute fish
        // script.
        // Suppress wildcards because we want to suggest e.g. `rm *` even if the directory
        // is empty (and so rm will fail); this is nevertheless a useful command because it
        // confirms the directory is empty.
        let mut expanded_path = path.clone();
        if expand_one(
            &mut expanded_path,
            ExpandFlags::FAIL_ON_CMDSUBST | ExpandFlags::SKIP_WILDCARDS,
            &ctx,
            None,
        ) && path_is_valid(&expanded_path, &working_directory)
        {
            // Note we return the original (unexpanded) path.
            result.push(path);
        }
    }

    result
}

/// Given a list of proposed paths and a context, expand each one and see if it refers to a file.
/// Wildcard expansions are suppressed.
/// Returns `true` if `paths` is empty or every path is valid.
pub fn all_paths_are_valid<P: IntoIterator<Item = WString>>(
    paths: P,
    ctx: &OperationContext<'_>,
) -> bool {
    assert_is_background_thread();
    let working_directory = ctx.vars().get_pwd_slash();
    for mut path in paths {
        if ctx.check_cancel() {
            return false;
        }
        if !expand_one(
            &mut path,
            ExpandFlags::FAIL_ON_CMDSUBST | ExpandFlags::SKIP_WILDCARDS,
            ctx,
            None,
        ) {
            return false;
        }
        if !path_is_valid(&path, &working_directory) {
            return false;
        }
    }
    true
}

/// Sets private mode on. Once in private mode, it cannot be turned off.
pub fn start_private_mode(vars: &EnvStack) {
    vars.set_one(L!("fish_history"), EnvMode::GLOBAL, L!("").to_owned());
    vars.set_one(L!("fish_private_mode"), EnvMode::GLOBAL, L!("1").to_owned());
}

/// Queries private mode status.
pub fn in_private_mode(vars: &dyn Environment) -> bool {
    vars.get_unless_empty(L!("fish_private_mode")).is_some()
}

/// Whether to force the read path instead of mmap. This is useful for testing.
static NEVER_MMAP: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Whether we're in maximum chaos mode, useful for testing.
/// This causes things like locks to fail.
pub static CHAOS_MODE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
