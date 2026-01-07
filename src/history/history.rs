//! Fish supports multiple shells writing to history at once. Here is its strategy:
//!
//! 1. All history files are append-only. Data, once written, is never modified.
//!
//! 2. A history file may be re-written ("vacuumed"). This involves reading in the file and writing
//!    a new one, while performing maintenance tasks: discarding items in an LRU fashion until we
//!    reach the desired maximum count, removing duplicates, and sorting them by timestamp
//!    (eventually, not implemented yet). The new file is atomically moved into place via `rename()`.
//!
//! 3. History files are mapped in via `mmap()`. This allows only storing one `usize` per item (its
//!    offset), and lazily loading items on demand, which reduces memory consumption.
//!
//! 4. Accesses to the history file need to be synchronized. This is achieved by functionality in
//!    `src/fs.rs`. By default, `flock()` is used for locking. If that is unavailable, an imperfect
//!    fallback solution attempts to detect races and retries if a race is detected.

use crate::{
    common::cstr2wcstring,
    env::{EnvSetMode, EnvVar},
    fs::{
        LOCKED_FILE_MODE, LockedFile, LockingMode, PotentialUpdate, WriteMethod, lock_and_load,
        rewrite_via_temporary_file,
    },
    threads::ThreadPool,
    wcstringutil::trim,
};
use std::{
    borrow::Cow,
    collections::{BTreeMap, HashMap, HashSet},
    ffi::CString,
    fs::File,
    io::{BufRead, BufWriter, Read, Write},
    mem::MaybeUninit,
    num::NonZeroUsize,
    ops::ControlFlow,
    sync::{Arc, Mutex, MutexGuard},
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use crate::global_safety::AtomicU64;
use std::sync::atomic::Ordering;

use bitflags::bitflags;
use lru::LruCache;
use nix::{fcntl::OFlag, sys::stat::Mode};
use rand::Rng;

use crate::{
    ast::{self, Kind, Node},
    common::{CancelChecker, UnescapeStringStyle, bytes2wcstring, unescape_string, valid_var_name},
    env::{EnvMode, EnvStack, Environment},
    expand::{ExpandFlags, expand_one},
    fds::wopen_cloexec,
    flog::{flog, flogf},
    fs::fsync,
    history::file::{HistoryFile, RawHistoryFile},
    io::IoStreams,
    localization::wgettext_fmt,
    operation_context::{EXPANSION_LIMIT_BACKGROUND, OperationContext},
    parse_constants::{ParseTreeFlags, StatementDecoration},
    parse_util::{parse_util_detect_errors, parse_util_unescape_wildcards},
    path::{path_get_config, path_get_data, path_is_valid},
    prelude::*,
    threads::assert_is_background_thread,
    util::find_subslice,
    wcstringutil::subsequence_in_string,
    wildcard::{ANY_STRING, wildcard_match},
    wutil::{FileId, INVALID_FILE_ID, file_id_for_file, wrealpath, wstat, wunlink},
};

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

use super::file::time_to_seconds;

/// This is the history session ID we use by default if the user has not set env var fish_history.
const DFLT_FISH_HISTORY_SESSION_ID: &wstr = L!("fish");

pub const VACUUM_FREQUENCY: usize = 25;

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
            flogf!(
                profile_history,
                "%s: %d.%06d ms",
                self.what,
                ms as u64, // todo!("remove cast")
                ns as u32
            )
        } else {
            flogf!(profile_history, "%s: ??? ms", self.what)
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

pub type PathList = Vec<WString>;

#[derive(Clone, Debug)]
pub struct HistoryItem {
    /// The actual contents of the entry.
    contents: WString,
    /// Original creation time for the entry.
    creation_timestamp: SystemTime,
    /// Paths that we require to be valid for this item to be autosuggested.
    required_paths: Vec<WString>,
    /// Whether to write this item to disk.
    persist_mode: PersistenceMode,
}

impl HistoryItem {
    /// Construct from a text, timestamp, and optional identifier.
    /// If `persist_mode` is not [`PersistenceMode::Disk`], then do not write this item to disk.
    pub fn new(
        s: WString,
        when: SystemTime,              /*=0*/
        persist_mode: PersistenceMode, /*=Disk*/
    ) -> Self {
        Self {
            contents: s,
            creation_timestamp: when,
            required_paths: vec![],
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
        true
    }
}

static HISTORIES: Mutex<BTreeMap<WString, Arc<History>>> = Mutex::new(BTreeMap::new());

/// When deleting, whether the deletion should be only for this session or for all sessions.
#[derive(Clone, Copy, PartialEq, Eq)]
enum DeletionScope {
    SessionOnly,
    AllSessions,
}

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
    /// Deleted item contents, and the scope of the deletion.
    deleted_items: HashMap<WString, DeletionScope>,
    /// The history file contents.
    file_contents: Option<HistoryFile>,
    /// The file ID of the history file.
    history_file_id: FileId, // INVALID_FILE_ID
    /// The boundary timestamp distinguishes old items from new items. Items whose timestamps are <=
    /// the boundary are considered "old". Items whose timestamps are > the boundary are new, and are
    /// ignored by this instance (unless they came from this instance). The timestamp may be adjusted
    /// by incorporate_external_changes().
    boundary_timestamp: SystemTime,
    /// How many items we add until the next vacuum. Initially a random value.
    countdown_to_vacuum: Option<usize>,
    /// Thread pool for background operations.
    thread_pool: Arc<ThreadPool>,
}

impl HistoryImpl {
    /// Returns the canonical path for the history file, or `Ok(None)` in private mode.
    /// An error is returned if obtaining the data directory fails.
    /// Because the `path_get_data` function does not return error information,
    /// we cannot provide more detail about the reason for the failure here.
    fn history_file_path(&self) -> std::io::Result<Option<WString>> {
        if self.name.is_empty() {
            return Ok(None);
        }

        let Some(mut path) = path_get_data() else {
            return Err(std::io::Error::new(
                std::io::ErrorKind::NotFound,
                "Error obtaining data directory. This is a manually constructed error which does not indicate why this happened.",
            ));
        };

        path.push('/');
        path.push_utfstr(&self.name);
        path.push_utfstr(L!("_history"));
        if let Some(canonicalized_path) = wrealpath(&path) {
            Ok(Some(canonicalized_path))
        } else {
            Err(std::io::Error::other(format!(
                "wrealpath failed to produce a canonical version of '{path}'."
            )))
        }
    }

    /// Add a new history item to the end. If `pending` is set, the item will not be returned by
    /// `item_at_index()` until a call to `resolve_pending()`. Pending items are tracked with an
    /// offset into the array of new items, so adding a non-pending item has the effect of resolving
    /// all pending items.
    fn add(&mut self, item: HistoryItem, pending: bool, do_save: bool) {
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

    /// Loads old items if necessary.
    /// Return a reference to the loaded history file.
    fn load_old_if_needed(&mut self) -> &HistoryFile {
        if let Some(ref file_contents) = self.file_contents {
            return file_contents;
        }
        let Ok(Some(history_path)) = self.history_file_path() else {
            return self.file_contents.insert(HistoryFile::create_empty());
        };

        let _profiler = TimeProfiler::new("load_old");
        let file_contents = match lock_and_load(&history_path, RawHistoryFile::create) {
            Ok((file_id, history_file)) => {
                self.history_file_id = file_id;
                let _profiler = TimeProfiler::new("populate_from_file_contents");
                let file_contents = history_file.decode(Some(self.boundary_timestamp));
                flogf!(
                    history,
                    "Loaded %u old items",
                    file_contents.offsets().len()
                );
                file_contents
            }
            Err(e) => {
                flog!(history_file, "Error reading from history file:", e);
                HistoryFile::create_empty()
            }
        };
        self.file_contents.insert(file_contents)
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
    fn rewrite_to_temporary_file(
        &self,
        existing_file: &File,
        dst: &mut File,
    ) -> std::io::Result<()> {
        // We are reading FROM existing_file and writing TO dst

        // Make an LRU cache to save only the last N elements.

        /// When we rewrite the history, the number of items we keep.
        const HISTORY_SAVE_MAX: NonZeroUsize = NonZeroUsize::new(1024 * 256).unwrap();
        let mut lru = LruCache::new(HISTORY_SAVE_MAX);

        // Read in existing items (which may have changed out from underneath us, so don't trust our
        // old file contents).
        let file_id = file_id_for_file(existing_file);
        if let Ok(local_file) = RawHistoryFile::create(existing_file, file_id) {
            for offset in local_file.offsets(None) {
                // Try decoding an old item.
                let Some(old_item) = local_file.decode_item(offset) else {
                    continue;
                };
                if old_item.is_empty() {
                    continue;
                }

                // Check if this item should be deleted.
                if let Some(&scope) = self.deleted_items.get(old_item.str()) {
                    // If old item is newer than session always erase if in deleted.
                    // If old item is older and in deleted items don't erase if added by clear_session.
                    let delete = old_item.timestamp() > self.boundary_timestamp
                        || scope == DeletionScope::AllSessions;
                    if delete {
                        continue;
                    }
                }
                lru.add_item(old_item);
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

        /// Default buffer size for flushing to the history file.
        const HISTORY_OUTPUT_BUFFER_SIZE: usize = 64 * 1024;
        // Write them out.
        let mut buffer = BufWriter::with_capacity(HISTORY_OUTPUT_BUFFER_SIZE + 128, dst);
        for item in items {
            item.write_to(&mut buffer)?;
        }
        buffer.flush()?;
        Ok(())
    }

    /// Saves history by rewriting the file.
    fn save_internal_via_rewrite(&mut self, history_path: &wstr) -> std::io::Result<()> {
        flogf!(
            history,
            "Saving %u items via rewrite",
            self.new_items.len() - self.first_unwritten_new_item_index
        );

        let rewrite =
            |old_file: &File, tmp_file: &mut File| -> std::io::Result<PotentialUpdate<()>> {
                let result = self.rewrite_to_temporary_file(old_file, tmp_file);
                if let Err(err) = result {
                    flog!(
                        history_file,
                        "Error writing to temporary history file:",
                        err
                    );
                    return Err(err);
                }
                Ok(PotentialUpdate {
                    do_save: true,
                    data: (),
                })
            };

        let (file_id, _) = rewrite_via_temporary_file(history_path, rewrite)?;
        self.history_file_id = file_id;

        // We've saved everything, so we have no more unsaved items.
        self.first_unwritten_new_item_index = self.new_items.len();

        // We deleted our deleted items.
        self.deleted_items.clear();

        // Our history has been written to the file, so clear our state so we can re-reference the
        // file.
        self.clear_file_state();

        Ok(())
    }

    /// Saves history by appending to the file.
    fn save_internal_via_appending(&mut self, history_path: &wstr) -> std::io::Result<()> {
        flogf!(
            history,
            "Saving %u items via appending",
            self.new_items.len() - self.first_unwritten_new_item_index
        );
        // No deleting allowed.
        assert!(self.deleted_items.is_empty());

        let mut locked_history_file =
            LockedFile::new(LockingMode::Exclusive(WriteMethod::Append), history_path)?;

        // Check if the file was modified since it was last read.
        // If someone has replaced the file, forget our file state.
        if file_id_for_file(locked_history_file.get()) != self.history_file_id {
            self.clear_file_state();
        }

        // We took the exclusive lock. Append to the file.
        // Note that this is sketchy for a few reasons:
        //   - Another shell may have appended its own items with a later timestamp, so our file may
        // no longer be sorted by timestamp.
        //   - Another shell may have appended the same items, so our file may now contain
        // duplicates.
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
        // Use a small buffer size for appending, as we usually only have 1 item.
        // Buffer everything and then write it all at once to avoid tearing writes (O_APPEND).
        let mut buffer = Vec::new();
        let mut new_first_index = self.first_unwritten_new_item_index;
        while new_first_index < self.new_items.len() {
            let item = &self.new_items[new_first_index];
            if item.should_write_to_disk() {
                // Can't error writing to a buffer.
                item.write_to(&mut buffer).unwrap();
            }
            // We wrote or skipped this item, hooray.
            new_first_index += 1;
        }
        locked_history_file.get_mut().write_all(&buffer)?;
        fsync(locked_history_file.get())?;
        self.first_unwritten_new_item_index = new_first_index;

        // Since we just modified the file, update our history_file_id to match its current state
        // Otherwise we'll think the file has been changed by someone else the next time we go to
        // write.
        // We don't update `self.file_contents` since we only appended to the file, and everything we
        // appended remains in our new_items
        self.history_file_id = file_id_for_file(locked_history_file.get());

        Ok(())
    }

    /// Saves history.
    fn save(&mut self, vacuum: bool) {
        // Nothing to do if there's no new items.
        if self.first_unwritten_new_item_index >= self.new_items.len()
            && self.deleted_items.is_empty()
        {
            return;
        }

        // Compact our new items so we don't have duplicates.
        self.compact_new_items();

        if self.name.is_empty() {
            // We're in the "incognito" mode. Pretend we've saved the history.
            self.first_unwritten_new_item_index = self.new_items.len();
            self.deleted_items.clear();
            self.clear_file_state();
            return;
        }

        let history_path = match self.history_file_path() {
            Ok(history_path) => history_path.unwrap(),
            Err(e) => {
                flog!(history, "Saving history failed:", e);
                return;
            }
        };

        // Try saving. If we have items to delete, we have to rewrite the file. If we do not, we can
        // append to it.
        let mut ok = false;
        if !vacuum && self.deleted_items.is_empty() {
            // Try doing a fast append.
            if let Err(e) = self.save_internal_via_appending(&history_path) {
                flog!(history, "Appending to history failed:", e);
            } else {
                ok = true;
            }
        }
        if !ok {
            // We did not or could not append; rewrite the file ("vacuum" it).
            if let Err(e) = self.save_internal_via_rewrite(&history_path) {
                flog!(history, "Rewriting history failed:", e)
            }
        }
    }

    /// Saves history unless doing so is disabled.
    fn save_unless_disabled(&mut self) {
        // Respect disable_automatic_save_counter.
        if self.disable_automatic_save_counter > 0 {
            return;
        }

        // We may or may not vacuum. We try to vacuum every `VACUUM_FREQUENCY` items, but start the
        // countdown at a random number so that even if the user never runs more than 25 commands, we'll
        // eventually vacuum.  If countdown_to_vacuum is None, it means we haven't yet picked a value for
        // the counter.
        let countdown_to_vacuum = self
            .countdown_to_vacuum
            .get_or_insert_with(|| rand::rng().random_range(0..VACUUM_FREQUENCY));

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
            countdown_to_vacuum: None,
            // Up to 8 threads, no soft min.
            thread_pool: ThreadPool::new(0, 8),
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

        if let Some(file_contents) = &self.file_contents {
            // If we've loaded old items, see if we have any offsets.
            file_contents.is_empty()
        } else {
            // If we have not loaded old items, don't actually load them (which may be expensive); just
            // stat the file and see if it exists and is nonempty.

            let Ok(Some(where_)) = self.history_file_path() else {
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
        self.deleted_items
            .insert(str_to_remove.to_owned(), DeletionScope::AllSessions);

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
        self.file_contents = None;
        if let Ok(Some(filename)) = self.history_file_path() {
            wunlink(&filename);
        }
        self.clear_file_state();
    }

    /// Clears only session.
    fn clear_session(&mut self) {
        for item in &self.new_items {
            self.deleted_items
                .insert(item.str().to_owned(), DeletionScope::SessionOnly);
        }

        self.new_items.clear();
        self.first_unwritten_new_item_index = 0;
    }

    /// Populates from older location (in config path, rather than data path).
    /// This is accomplished by clearing ourselves, and copying the contents of the old history
    /// file to the new history file.
    /// The new contents will automatically be re-mapped later.
    fn populate_from_config_path(&mut self) {
        let Ok(Some(new_file)) = self.history_file_path() else {
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
            LOCKED_FILE_MODE,
        ) {
            Ok(file) => file,
            Err(err) => {
                flog!(history_file, "Error when writing history file:", err);
                return;
            }
        };

        let mut buf = [0; libc::BUFSIZ as usize];
        while let Ok(n) = src_file.read(&mut buf) {
            if n == 0 {
                break;
            }

            if let Err(err) = dst_file.write_all(&buf[..n]) {
                flog!(history_file, "Error when writing history file:", err);
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
            let wide_line = trim(bytes2wcstring(&line), None);
            // Add this line if it doesn't contain anything we know we can't handle.
            if should_import_bash_history_line(&wide_line) {
                self.add(
                    HistoryItem::new(wide_line, when, PersistenceMode::Disk),
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
        let file_contents = self.load_old_if_needed();
        for &offset in file_contents.offsets().iter().rev() {
            let Some(item) = file_contents.decode_item(offset) else {
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

    /// Sets the valid file paths for the history item matching the snapshotted item.
    fn set_valid_file_paths(&mut self, valid_file_paths: Vec<WString>, snapshot: &HistoryItem) {
        // Look for an item with the given identifier. It is likely to be at the end of new_items.
        for item in self.new_items.iter_mut().rev() {
            if item.creation_timestamp == snapshot.creation_timestamp
                && item.contents == snapshot.contents
            {
                // found it
                item.required_paths = valid_file_paths;
                break;
            }
        }
    }

    /// Return the specified history at the specified index. 0 is the index of the current
    /// commandline. (So the most recent item is at index 1.)
    fn item_at_index(&mut self, mut idx: usize) -> Option<Cow<'_, HistoryItem>> {
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
        let file_contents = self.load_old_if_needed();
        let old_item_offsets = file_contents.offsets();
        let old_item_count = old_item_offsets.len();
        if idx < old_item_count {
            // idx == 0 corresponds to last item in old_item_offsets.
            let offset = old_item_offsets[old_item_count - idx - 1];
            return file_contents.decode_item(offset).map(Cow::Owned);
        }

        // Index past the valid range, so return None.
        None
    }

    /// Return the number of history entries.
    fn size(&mut self) -> usize {
        let mut new_item_count = self.new_items.len();
        if self.has_pending_item && new_item_count > 0 {
            new_item_count -= 1;
        }
        let old_item_offsets = self.load_old_if_needed().offsets();
        new_item_count + old_item_offsets.len()
    }
}

fn string_could_be_path(potential_path: &wstr) -> bool {
    // Assume that things with leading dashes aren't paths.
    !(potential_path.is_empty() || potential_path.starts_with('-'))
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
            const MAX_TIMESTAMP_LENGTH: usize = 100;
            let mut timestamp_str = [0_u8; MAX_TIMESTAMP_LENGTH];
            if unsafe {
                libc::strftime(
                    timestamp_str.as_mut_ptr().cast(),
                    MAX_TIMESTAMP_LENGTH,
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

pub struct History {
    imp: Mutex<HistoryImpl>,
    generation: AtomicU64,
}

impl History {
    fn imp(&self) -> MutexGuard<'_, HistoryImpl> {
        self.imp.lock().unwrap()
    }

    pub fn generation(&self) -> u64 {
        self.generation.load(Ordering::Acquire)
    }

    fn bump_generation(&self) {
        self.generation.fetch_add(1, Ordering::Release);
    }

    /// Privately add an item. If pending, the item will not be returned by history searches until a
    /// call to resolve_pending. Any trailing ephemeral items are dropped.
    /// Exposed for testing.
    pub fn add(&self, item: HistoryItem, pending: bool) {
        self.imp().add(item, pending, true);
        self.bump_generation();
    }

    pub fn add_commandline(&self, s: WString) {
        let mut imp = self.imp();
        let when = imp.timestamp_now();
        let item = HistoryItem::new(s, when, PersistenceMode::Disk);
        imp.add(item, false, true);
        drop(imp);
        self.bump_generation();
    }

    pub fn new(name: &wstr) -> Arc<Self> {
        Arc::new(Self {
            imp: Mutex::new(HistoryImpl::new(name.to_owned())),
            generation: AtomicU64::new(0),
        })
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
        self.imp().remove(s);
        self.bump_generation();
    }

    /// Remove any trailing ephemeral items.
    pub fn remove_ephemeral_items(&self) {
        self.imp().remove_ephemeral_items();
        self.bump_generation();
    }

    /// Add a new pending history item to the end, and then begin file detection on the items to
    /// determine which arguments are paths. Arguments may be expanded (e.g. with PWD and variables)
    /// using the given `vars`. The item has the given `persist_mode`.
    pub fn add_pending_with_file_detection(
        self: &Arc<Self>,
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
                if stmt.decoration() == StatementDecoration::Exec {
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
        let item = HistoryItem::new(s.to_owned(), when, persist_mode);
        let to_disk = persist_mode == PersistenceMode::Disk;

        if wants_file_detection {
            imp.disable_automatic_saving();

            // Add the item. Then check for which paths are valid on a background thread,
            // and unblock the item.
            // Don't hold the lock while we perform this file detection.
            let snapshot_item = item.clone();
            imp.add(item, /*pending=*/ true, to_disk);
            let thread_pool = Arc::clone(&imp.thread_pool);
            drop(imp);
            let vars_snapshot = vars.snapshot();
            let self_clone = Arc::clone(self);
            thread_pool.perform(move || {
                // Don't hold the lock while we perform this file detection.
                let valid_file_paths = expand_and_detect_paths(potential_paths, &vars_snapshot);
                let mut imp = self_clone.imp();
                if !valid_file_paths.is_empty() {
                    imp.set_valid_file_paths(valid_file_paths, &snapshot_item);
                }
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
        self.imp().resolve_pending();
        self.bump_generation();
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
                if !streams.out.append(&formatted_record) {
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

            if !streams.out.append(&item) {
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
        self.imp().clear();
        self.bump_generation();
    }

    /// Irreversibly clears history for the current session.
    pub fn clear_session(&self) {
        self.imp().clear_session();
        self.bump_generation();
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
        self.imp().incorporate_external_changes();
        self.bump_generation();
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
    /// The history generation at the time this search was created.
    /// Used for staleness detection.
    captured_generation: u64,
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
        let captured_generation = hist.generation();
        let mut search = Self {
            history: hist,
            orig_term: s.clone(),
            canon_term: s,
            search_type,
            flags,
            current_item: None,
            current_index: starting_index,
            deduper: HashSet::new(),
            captured_generation,
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

    /// Returns whether the underlying history has been modified since this search was created.
    /// If true, the search results may be stale and the search should be reset.
    pub fn is_stale(&self) -> bool {
        self.history.generation() != self.captured_generation
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
        flog!(
            error,
            wgettext_fmt!(
                "History session ID '%s' is not a valid variable name. Falling back to `%s`.",
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
    let global_mode = EnvSetMode::new_at_early_startup(EnvMode::GLOBAL);
    vars.set_one(L!("fish_history"), global_mode, L!("").to_owned());
    vars.set_one(L!("fish_private_mode"), global_mode, L!("1").to_owned());
}

/// Queries private mode status.
pub fn in_private_mode(vars: &dyn Environment) -> bool {
    vars.get_unless_empty(L!("fish_private_mode")).is_some()
}

#[cfg(test)]
mod tests {
    use super::{
        History, HistoryItem, HistorySearch, PathList, PersistenceMode, SearchDirection,
        SearchFlags, SearchType, VACUUM_FREQUENCY,
    };
    use crate::common::ESCAPE_TEST_CHAR;
    use crate::common::{ScopeGuard, bytes2wcstring, wcs2bytes, wcs2osstring};
    use crate::env::{EnvMode, EnvSetMode, EnvStack};
    use crate::fs::{LockedFile, WriteMethod};
    use crate::path::path_get_data;
    use crate::prelude::*;
    use crate::tests::prelude::*;
    use crate::wcstringutil::{string_prefixes_string, string_prefixes_string_case_insensitive};
    use fish_build_helper::workspace_root;
    use rand::Rng;
    use rand::rngs::ThreadRng;
    use std::collections::VecDeque;
    use std::io::BufReader;
    use std::os::unix::ffi::OsStrExt;
    use std::sync::Arc;
    use std::time::UNIX_EPOCH;
    use std::time::{Duration, SystemTime};

    fn history_contains(history: &History, txt: &wstr) -> bool {
        for i in 1.. {
            let Some(item) = history.item_at_index(i) else {
                break;
            };

            if item.str() == txt {
                return true;
            }
        }

        false
    }

    fn random_string(rng: &mut ThreadRng) -> WString {
        let mut result = WString::new();
        let max = rng.random_range(1..=32);
        for _ in 0..max {
            let c =
                char::from_u32(u32::try_from(1 + rng.random_range(0..ESCAPE_TEST_CHAR)).unwrap())
                    .unwrap();
            result.push(c);
        }
        result
    }

    #[test]
    #[serial]
    fn test_history() {
        let _cleanup = test_init();
        macro_rules! test_history_matches {
            ($search:expr, $expected:expr) => {
                let expected: Vec<&wstr> = $expected;
                let mut found = vec![];
                while $search.go_to_next_match(SearchDirection::Backward) {
                    found.push($search.current_string().to_owned());
                }
                assert_eq!(expected, found);
            };
        }

        let items = [
            L!("Gamma"),
            L!("beta"),
            L!("BetA"),
            L!("Beta"),
            L!("alpha"),
            L!("AlphA"),
            L!("Alpha"),
            L!("alph"),
            L!("ALPH"),
            L!("ZZZ"),
        ];
        let nocase = SearchFlags::IGNORE_CASE;

        // Populate a history.
        let history = History::with_name(L!("test_history"));
        history.clear();
        for s in items {
            history.add_commandline(s.to_owned());
        }

        // Helper to set expected items to those matching a predicate, in reverse order.
        let set_expected = |filt: fn(&wstr) -> bool| {
            let mut expected = vec![];
            for s in items {
                if filt(s) {
                    expected.push(s);
                }
            }
            expected.reverse();
            expected
        };

        // Items matching "a", case-sensitive.
        let mut searcher = HistorySearch::new(history.clone(), L!("a").to_owned());
        let expected = set_expected(|s| s.contains('a'));
        test_history_matches!(searcher, expected);

        // Items matching "alpha", case-insensitive.
        let mut searcher =
            HistorySearch::new_with_flags(history.clone(), L!("AlPhA").to_owned(), nocase);
        let expected = set_expected(|s| s.to_lowercase().find(L!("alpha")).is_some());
        test_history_matches!(searcher, expected);

        // Items matching "et", case-sensitive.
        let mut searcher = HistorySearch::new(history.clone(), L!("et").to_owned());
        let expected = set_expected(|s| s.find(L!("et")).is_some());
        test_history_matches!(searcher, expected);

        // Items starting with "be", case-sensitive.
        let mut searcher =
            HistorySearch::new_with_type(history.clone(), L!("be").to_owned(), SearchType::Prefix);
        let expected = set_expected(|s| string_prefixes_string(L!("be"), s));
        test_history_matches!(searcher, expected);

        // Items starting with "be", case-insensitive.
        let mut searcher = HistorySearch::new_with(
            history.clone(),
            L!("be").to_owned(),
            SearchType::Prefix,
            nocase,
            0,
        );
        let expected = set_expected(|s| string_prefixes_string_case_insensitive(L!("be"), s));
        test_history_matches!(searcher, expected);

        // Items exactly matching "alph", case-sensitive.
        let mut searcher =
            HistorySearch::new_with_type(history.clone(), L!("alph").to_owned(), SearchType::Exact);
        let expected = set_expected(|s| s == "alph");
        test_history_matches!(searcher, expected);

        // Items exactly matching "alph", case-insensitive.
        let mut searcher = HistorySearch::new_with(
            history.clone(),
            L!("alph").to_owned(),
            SearchType::Exact,
            nocase,
            0,
        );
        let expected = set_expected(|s| s.to_lowercase() == "alph");
        test_history_matches!(searcher, expected);

        // Test item removal case-sensitive.
        let mut searcher = HistorySearch::new(history.clone(), L!("Alpha").to_owned());
        test_history_matches!(searcher, vec![L!("Alpha")]);
        history.remove(L!("Alpha"));
        let mut searcher = HistorySearch::new(history.clone(), L!("Alpha").to_owned());
        test_history_matches!(searcher, vec![]);

        // Test history escaping and unescaping, yaml, etc.
        let mut before: VecDeque<HistoryItem> = VecDeque::new();
        let mut after: VecDeque<HistoryItem> = VecDeque::new();
        history.clear();
        let max = 100;
        let mut rng = rand::rng();
        for i in 1..=max {
            // Generate a value.
            let mut value = WString::from_str("test item ") + &i.to_wstring()[..];

            // Maybe add some backslashes.
            if i % 3 == 0 {
                value += L!("(slashies \\\\\\ slashies)");
            }

            // Generate some paths.
            let paths: PathList = (0..rng.random_range(0..6))
                .map(|_| random_string(&mut rng))
                .collect();

            // Record this item.
            let mut item = HistoryItem::new(value, SystemTime::now(), PersistenceMode::Disk);
            item.set_required_paths(paths);
            before.push_back(item.clone());
            history.add(item, false);
        }
        history.save();

        // Empty items should just be dropped (#6032).
        history.add_commandline(L!("").into());
        assert!(!history.item_at_index(1).unwrap().is_empty());

        // Read items back in reverse order and ensure they're the same.
        for i in (1..=100).rev() {
            after.push_back(history.item_at_index(i).unwrap());
        }
        assert_eq!(before.len(), after.len());
        for i in 0..before.len() {
            let bef = &before[i];
            let aft = &after[i];
            assert_eq!(bef.str(), aft.str());
            assert_eq!(bef.timestamp(), aft.timestamp());
            assert_eq!(bef.get_required_paths(), aft.get_required_paths());
        }

        // Items should be explicitly added to the history.
        history.add_commandline(L!("test-command").into());
        assert!(history_contains(&history, L!("test-command")));

        // Clean up after our tests.
        history.clear();
    }

    // Wait until the next second.
    fn time_barrier() {
        let start = SystemTime::now();
        loop {
            std::thread::sleep(std::time::Duration::from_millis(1));
            if SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_secs()
                != start.duration_since(UNIX_EPOCH).unwrap().as_secs()
            {
                break;
            }
        }
    }

    fn generate_history_lines(item_count: usize, idx: usize) -> Vec<WString> {
        let mut result = Vec::with_capacity(item_count);
        for i in 0..item_count {
            result.push(sprintf!("%u %u", idx, i));
        }
        result
    }

    fn pound_on_history(item_count: usize, idx: usize) -> Arc<History> {
        // Called in child thread to modify history.
        let hist = History::new(L!("race_test"));
        let hist_lines = generate_history_lines(item_count, idx);
        for line in hist_lines {
            hist.add_commandline(line);
            hist.save();
        }
        hist
    }

    #[test]
    #[serial]
    fn test_history_races() {
        let _cleanup = test_init();

        let tmp_path = std::env::current_dir()
            .unwrap()
            .join("history-races-test-balloon");
        std::fs::write(&tmp_path, []).unwrap();
        let _cleanup = ScopeGuard::new((), |()| {
            std::fs::remove_file(&tmp_path).unwrap();
        });
        if LockedFile::new(
            crate::fs::LockingMode::Exclusive(WriteMethod::RenameIntoPlace),
            &bytes2wcstring(tmp_path.as_os_str().as_bytes()),
        )
        .is_err()
        {
            return;
        }

        // Testing history race conditions

        // Test concurrent history writing.
        // How many concurrent writers we have
        const RACE_COUNT: usize = 4;

        // How many items each writer makes
        const ITEM_COUNT: usize = 256;

        // Ensure history is clear.
        History::new(L!("race_test")).clear();

        let mut children = Vec::with_capacity(RACE_COUNT);
        for i in 0..RACE_COUNT {
            children.push(std::thread::spawn(move || {
                pound_on_history(ITEM_COUNT, i);
            }));
        }

        // Wait for all children.
        for child in children {
            child.join().unwrap();
        }

        // Compute the expected lines.
        let mut expected_lines: [Vec<WString>; RACE_COUNT] =
            std::array::from_fn(|i| generate_history_lines(ITEM_COUNT, i));

        // Ensure we consider the lines that have been outputted as part of our history.
        time_barrier();

        // Ensure that we got sane, sorted results.
        let hist = History::new(L!("race_test"));

        // History is enumerated from most recent to least
        // Every item should be the last item in some array
        let mut hist_idx = 0;
        loop {
            hist_idx += 1;
            let Some(item) = hist.item_at_index(hist_idx) else {
                break;
            };

            let mut found = false;
            for list in &mut expected_lines {
                let Some(position) = list.iter().position(|elem| *elem == item.str()) else {
                    continue;
                };

                // Remove the item we found.
                list.remove(position);

                // We expected this item to be the last. Items after this item
                // in this array were therefore not found in history.
                let removed = list.drain(position..);
                for line in removed.into_iter().rev() {
                    printf!("Item dropped from history: %s\n", line);
                }

                found = true;
                break;
            }
            if !found {
                printf!(
                    "Line '%s' found in history, but not found in some array\n",
                    item.str()
                );
                for list in &expected_lines {
                    if !list.is_empty() {
                        printf!("\tRemaining: %s\n", list.last().unwrap())
                    }
                }
            }
        }

        // +1 to account for history's 1-based offset
        let expected_idx = RACE_COUNT * ITEM_COUNT + 1;
        assert_eq!(hist_idx, expected_idx);

        for list in expected_lines {
            assert_eq!(list, Vec::<WString>::new(), "Lines still left in the array");
        }
        hist.clear();
    }

    #[test]
    #[serial]
    fn test_history_external_rewrites() {
        let _cleanup = test_init();

        // Write some history to disk.
        {
            let hist = pound_on_history(VACUUM_FREQUENCY / 2, 0);
            hist.add_commandline("needle".into());
            hist.save();
        }
        std::thread::sleep(Duration::from_secs(1));

        // Read history from disk.
        let hist = History::new(L!("race_test"));
        assert_eq!(hist.item_at_index(1).unwrap().str(), "needle");

        // Add items until we rewrite the file.
        // In practice this might be done by another shell.
        pound_on_history(VACUUM_FREQUENCY, 0);

        for i in 1.. {
            if hist.item_at_index(i).unwrap().str() == "needle" {
                break;
            }
        }
    }

    #[test]
    #[serial]
    fn test_history_merge() {
        let _cleanup = test_init();
        // In a single fish process, only one history is allowed to exist with the given name But it's
        // common to have multiple history instances with the same name active in different processes,
        // e.g. when you have multiple shells open. We try to get that right and merge all their history
        // together. Test that case.
        const COUNT: usize = 3;
        let name = L!("merge_test");
        let hists = [History::new(name), History::new(name), History::new(name)];
        let texts = [L!("History 1"), L!("History 2"), L!("History 3")];
        let alt_texts = [
            L!("History Alt 1"),
            L!("History Alt 2"),
            L!("History Alt 3"),
        ];

        // Make sure history is clear.
        for hist in &hists {
            hist.clear();
        }

        // Make sure we don't add an item in the same second as we created the history.
        time_barrier();

        // Add a different item to each.
        for i in 0..COUNT {
            hists[i].add_commandline(texts[i].to_owned());
        }

        // Save them.
        for hist in &hists {
            hist.save()
        }

        // Make sure each history contains what it ought to, but they have not leaked into each other.
        #[allow(clippy::needless_range_loop)]
        for i in 0..COUNT {
            for j in 0..COUNT {
                let does_contain = history_contains(&hists[i], texts[j]);
                let should_contain = i == j;
                assert_eq!(should_contain, does_contain);
            }
        }

        // Make a new history. It should contain everything. The time_barrier() is so that the timestamp
        // is newer, since we only pick up items whose timestamp is before the birth stamp.
        time_barrier();
        let everything = History::new(name);
        for text in texts {
            assert!(history_contains(&everything, text));
        }

        // Tell all histories to merge. Now everybody should have everything.
        for hist in &hists {
            hist.incorporate_external_changes();
        }

        // Everyone should also have items in the same order (#2312)
        let hist_vals1 = hists[0].get_history();
        for hist in &hists {
            assert_eq!(hist_vals1, hist.get_history());
        }

        // Add some more per-history items.
        for i in 0..COUNT {
            hists[i].add_commandline(alt_texts[i].to_owned());
        }
        // Everybody should have old items, but only one history should have each new item.
        #[allow(clippy::needless_range_loop)]
        for i in 0..COUNT {
            for j in 0..COUNT {
                // Old item.
                assert!(history_contains(&hists[i], texts[j]));

                // New item.
                let does_contain = history_contains(&hists[i], alt_texts[j]);
                let should_contain = i == j;
                assert_eq!(should_contain, does_contain);
            }
        }

        // Make sure incorporate_external_changes doesn't drop items! (#3496)
        let writer = &hists[0];
        let reader = &hists[1];
        let more_texts = [
            L!("Item_#3496_1"),
            L!("Item_#3496_2"),
            L!("Item_#3496_3"),
            L!("Item_#3496_4"),
            L!("Item_#3496_5"),
            L!("Item_#3496_6"),
        ];
        for i in 0..more_texts.len() {
            // time_barrier because merging will ignore items that may be newer
            if i > 0 {
                time_barrier();
            }
            writer.add_commandline(more_texts[i].to_owned());
            writer.incorporate_external_changes();
            reader.incorporate_external_changes();
            for text in more_texts.iter().take(i) {
                assert!(history_contains(reader, text));
            }
        }
        everything.clear();
    }

    #[test]
    #[serial]
    fn test_history_path_detection() {
        let _cleanup = test_init();
        // Regression test for #7582.
        let tmpdir = fish_tempfile::new_dir().unwrap();

        // Place one valid file in the directory.
        let filename = L!("testfile");
        let file_path = tmpdir.path().join(filename.to_string());
        let wfile_path = WString::from(file_path.to_str().unwrap());
        std::fs::write(&file_path, []).unwrap();
        let wdir_path = WString::from(tmpdir.path().to_str().unwrap());

        let test_vars = EnvStack::new();
        let global_mode = EnvSetMode::new(EnvMode::GLOBAL, false);
        test_vars.set_one(L!("PWD"), global_mode, wdir_path.clone());
        test_vars.set_one(L!("HOME"), global_mode, wdir_path.clone());

        let history = History::with_name(L!("path_detection"));
        history.clear();
        assert_eq!(history.size(), 0);
        history.add_pending_with_file_detection(
            L!("cmd0 not/a/valid/path"),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            &(L!("cmd1 ").to_owned() + filename),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            &(L!("cmd2 ").to_owned() + &wfile_path[..]),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            &(L!("cmd3  $HOME/").to_owned() + filename),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            L!("cmd4  $HOME/notafile"),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            &(L!("cmd5  ~/").to_owned() + filename),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            L!("cmd6  ~/notafile"),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            L!("cmd7  ~/*f*"),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.add_pending_with_file_detection(
            L!("cmd8  ~/*zzz*"),
            &test_vars,
            PersistenceMode::Disk,
        );
        history.resolve_pending();

        const HIST_SIZE: usize = 9;
        assert_eq!(history.size(), 9);

        // Expected sets of paths.
        let expected_paths = [
            vec![],                                   // cmd0
            vec![filename.to_owned()],                // cmd1
            vec![wfile_path],                         // cmd2
            vec![L!("$HOME/").to_owned() + filename], // cmd3
            vec![],                                   // cmd4
            vec![L!("~/").to_owned() + filename],     // cmd5
            vec![],                                   // cmd6
            vec![],                                   // cmd7 - we do not expand globs
            vec![],                                   // cmd8
        ];

        let maxlap = 128;
        for _lap in 0..maxlap {
            let mut failures = 0;
            for i in 1..=HIST_SIZE {
                if history.item_at_index(i).unwrap().get_required_paths()
                    != expected_paths[HIST_SIZE - i]
                {
                    failures += 1;
                }
            }
            if failures == 0 {
                break;
            }
            // The file detection takes a little time since it occurs in the background.
            // Loop until the test passes.
            std::thread::sleep(std::time::Duration::from_millis(2));
        }
        history.clear();
    }

    fn install_sample_history(name: &wstr) {
        let path = path_get_data().expect("Failed to get data directory");
        std::fs::copy(
            workspace_root()
                .join("tests")
                .join(std::str::from_utf8(&wcs2bytes(name)).unwrap()),
            wcs2osstring(&(path + L!("/") + name + L!("_history"))),
        )
        .unwrap();
    }

    #[test]
    #[serial]
    fn test_history_formats() {
        let _cleanup = test_init();
        // Test inferring and reading legacy and bash history formats.
        let name = L!("history_sample_fish_2_0");
        install_sample_history(name);
        let expected: Vec<WString> = vec![
            "echo this has\\\nbackslashes".into(),
            "function foo\necho bar\nend".into(),
            "echo alpha".into(),
        ];
        let test_history_imported = History::with_name(name);
        assert_eq!(test_history_imported.get_history(), expected);
        test_history_imported.clear();

        // Test bash import
        // The results are in the reverse order that they appear in the bash history file.
        // We don't expect whitespace to be elided (#4908: except for leading/trailing whitespace)
        let expected: Vec<WString> = vec![
            "EOF".into(),
            "sleep 123".into(),
            "posix_cmd_sub $(is supported but only splits on newlines)".into(),
            "posix_cmd_sub \"$(is supported)\"".into(),
            "a && echo valid construct".into(),
            "final line".into(),
            "echo supsup".into(),
            "export XVAR='exported'".into(),
            "history --help".into(),
            "echo foo".into(),
        ];
        let test_history_imported_from_bash = History::with_name(L!("bash_import"));
        let file = std::fs::File::open(workspace_root().join("tests/history_sample_bash")).unwrap();
        test_history_imported_from_bash.populate_from_bash(BufReader::new(file));
        assert_eq!(test_history_imported_from_bash.get_history(), expected);
        test_history_imported_from_bash.clear();

        let name = L!("history_sample_corrupt1");
        install_sample_history(name);
        // We simply invoke get_string_representation. If we don't die, the test is a success.
        let test_history_imported_from_corrupted = History::with_name(name);
        let expected: Vec<WString> = vec![
            "no_newline_at_end_of_file".into(),
            "corrupt_prefix".into(),
            "this_command_is_ok".into(),
        ];
        assert_eq!(test_history_imported_from_corrupted.get_history(), expected);
        test_history_imported_from_corrupted.clear();
    }

    /// Helper to verify that a history operation bumps the generation counter by exactly 1
    /// and that an existing HistorySearch becomes stale after the operation.
    fn assert_bumps_generation<F>(history: &Arc<History>, operation_name: &str, operation: F)
    where
        F: FnOnce(),
    {
        let search = HistorySearch::new(Arc::clone(history), L!("test").to_owned());
        assert!(
            !search.is_stale(),
            "fresh search should not be stale before {}",
            operation_name
        );

        let gen_before = history.generation();
        operation();
        let gen_after = history.generation();

        assert_eq!(
            gen_after,
            gen_before + 1,
            "{} should bump generation by exactly 1",
            operation_name
        );
        assert!(
            search.is_stale(),
            "search should be stale after {}",
            operation_name
        );
    }

    #[test]
    #[serial]
    fn test_history_generation_counter() {
        // Test that the generation counter increments on history modifications
        // and that HistorySearch correctly detects staleness.
        // This is a regression test for https://github.com/fish-shell/fish-shell/issues/11696
        //
        // Tests all 8 methods that call bump_generation():
        // add(), add_commandline(), remove(), remove_ephemeral_items(),
        // resolve_pending(), clear(), clear_session(), incorporate_external_changes()
        let _cleanup = test_init();
        let history = History::with_name(L!("test_generation"));
        history.clear();

        // Test add() bumps generation
        let item = HistoryItem::new(
            L!("test command 1").to_owned(),
            UNIX_EPOCH,
            PersistenceMode::Memory,
        );
        assert_bumps_generation(&history, "add", || {
            history.add(item, false);
        });

        // Test incorporate_external_changes() bumps generation
        assert_bumps_generation(&history, "incorporate_external_changes", || {
            history.incorporate_external_changes();
        });

        // Test remove() bumps generation
        assert_bumps_generation(&history, "remove", || {
            history.remove(L!("test command 1"));
        });

        // Test clear_session() bumps generation
        assert_bumps_generation(&history, "clear_session", || {
            history.clear_session();
        });

        // Test resolve_pending() bumps generation (Critical - issue #11696)
        assert_bumps_generation(&history, "resolve_pending", || {
            history.resolve_pending();
        });

        // Test add_commandline() bumps generation
        assert_bumps_generation(&history, "add_commandline", || {
            history.add_commandline(L!("test commandline").to_owned());
        });

        // Test clear() bumps generation
        assert_bumps_generation(&history, "clear", || {
            history.clear();
        });

        // Test remove_ephemeral_items() bumps generation
        // First add an ephemeral item so there's something to remove
        let ephemeral_item = HistoryItem::new(
            L!("ephemeral command").to_owned(),
            UNIX_EPOCH,
            PersistenceMode::Ephemeral,
        );
        history.add(ephemeral_item, false);
        assert_bumps_generation(&history, "remove_ephemeral_items", || {
            history.remove_ephemeral_items();
        });

        history.clear();
    }
}
