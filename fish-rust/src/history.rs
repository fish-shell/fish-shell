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

use std::borrow::Cow;
use std::collections::HashMap;
use std::os::fd::RawFd;
use std::pin::Pin;
use std::sync::{Arc, Mutex, MutexGuard};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use bitflags::bitflags;
use cxx::{CxxWString, UniquePtr};
use widestring_suffix::widestrs;

use crate::common::write_loop;
use crate::env::{EnvDyn, EnvDynFFI, EnvStack, EnvStackRef, EnvStackRefFFI, Environment};
use crate::ffi::{wcstring_list_ffi_t, Repin};
use crate::flog::FLOGF;
use crate::io::IoStreams;
use crate::operation_context::OperationContext;
use crate::util::find_subslice;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{WCharFromFFI, WCharToFFI};
use crate::wcstringutil::subsequence_in_string;

#[cxx::bridge]
mod history_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("io.h");
        include!("env.h");
        include!("operation_context.h");

        type IoStreams<'a> = crate::io::IoStreams<'a>;
        #[cxx_name = "EnvDyn"]
        type EnvDynFFI = crate::env::EnvDynFFI;
        #[cxx_name = "EnvStackRef"]
        type EnvStackRefFFI = crate::env::EnvStackRefFFI;
        type OperationContext<'a> = crate::operation_context::OperationContext<'a>;
        type wcstring_list_ffi_t = crate::ffi::wcstring_list_ffi_t;
    }

    enum SearchType {
        /// Search for commands exactly matching the given string.
        Exact,
        /// Search for commands containing the given string.
        Contains,
        /// Search for commands starting with the given string.
        Prefix,
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
    enum PersistenceMode {
        /// The history item is written to disk normally
        Disk,
        /// The history item is stored in-memory only, not written to disk
        Memory,
        /// The history item is stored in-memory and deleted when a new item is added
        Ephemeral,
    }

    enum SearchDirection {
        Forward,
        Backward,
    }

    extern "Rust" {
        #[cxx_name = "history_save_all"]
        fn save_all();
        fn rust_session_id(vars: &EnvDynFFI) -> UniquePtr<CxxWString>;
        fn rust_expand_and_detect_paths(
            paths: &wcstring_list_ffi_t,
            vars: &EnvDynFFI,
        ) -> UniquePtr<wcstring_list_ffi_t>;
        fn rust_all_paths_are_valid(
            paths: &wcstring_list_ffi_t,
            ctx: &OperationContext<'_>,
        ) -> bool;
        #[rust_name = "start_private_mode_ffi"]
        fn start_private_mode(vars: &EnvStackRefFFI);
        #[rust_name = "in_private_mode_ffi"]
        fn in_private_mode(vars: &EnvDynFFI) -> bool;
        fn history_never_mmap() -> bool;
    }

    extern "Rust" {
        type ItemIndexes;

        fn get(&self, index: usize) -> UniquePtr<CxxWString>;
    }

    extern "Rust" {
        type HistoryItem;

        fn rust_history_item_new(
            s: &CxxWString,
            when: i64,
            ident: u64,
            persist_mode: PersistenceMode,
        ) -> Box<HistoryItem>;
        #[rust_name = "str_ffi"]
        fn str(&self) -> UniquePtr<CxxWString>;
        fn empty(&self) -> bool;
        #[rust_name = "matches_search_ffi"]
        fn matches_search(&self, term: &CxxWString, typ: SearchType, case_sensitive: bool) -> bool;
        fn timestamp(&self) -> i64;
        fn should_write_to_disk(&self) -> bool;
        #[rust_name = "get_required_paths_ffi"]
        fn get_required_paths(&self) -> UniquePtr<wcstring_list_ffi_t>;
        #[rust_name = "set_required_paths_ffi"]
        fn set_required_paths(&mut self, paths: &wcstring_list_ffi_t);
    }

    extern "Rust" {
        type HistorySharedPtr;
        fn history_with_name(name: &CxxWString) -> Box<HistorySharedPtr>;
        fn is_default(&self) -> bool;
        fn is_empty(&self) -> bool;
        fn remove(&self, s: &CxxWString);
        fn remove_ephemeral_items(&self);
        fn add_pending_with_file_detection(
            &self,
            s: &CxxWString,
            vars: &EnvDynFFI,
            persist_mode: PersistenceMode,
        );
        fn resolve_pending(&self);
        fn save(&self);
        fn search(
            &self,
            search_type: SearchType,
            search_args: &wcstring_list_ffi_t,
            show_time_format: &CxxWString,
            max_items: usize,
            case_sensitive: bool,
            null_terminate: bool,
            reverse: bool,
            cancel_on_signal: bool,
            streams: Pin<&mut IoStreams<'_>>,
        ) -> bool;
        fn clear(&self);
        fn clear_session(&self);
        fn populate_from_config_path(&self);
        fn populate_from_bash(&self, f: &CxxWString);
        fn incorporate_external_changes(&self);
        fn get_history(&self) -> UniquePtr<wcstring_list_ffi_t>;
        fn items_at_indexes(&self, indexes: &[isize]) -> Box<ItemIndexes>;
        fn item_at_index(&self, idx: usize) -> Box<HistoryItem>;
        fn size(&self) -> usize;
        fn clone(&self) -> Box<HistorySharedPtr>;
    }

    extern "Rust" {
        type HistorySearch;
        fn rust_history_search_new(
            hist: &HistorySharedPtr,
            s: &CxxWString,
            search_type: SearchType,
            flags: u32,
            starting_index: usize,
        ) -> Box<HistorySearch>;
        #[rust_name = "original_term_ffi"]
        fn original_term(&self) -> UniquePtr<CxxWString>;
        fn go_to_next_match(&mut self, direction: SearchDirection) -> bool;
        fn current_item(&self) -> &HistoryItem;
        #[rust_name = "current_string_ffi"]
        fn current_string(&self) -> UniquePtr<CxxWString>;
        fn current_index(&self) -> usize;
        fn ignores_case(&self) -> bool;
    }
}

use self::history_ffi::{PersistenceMode, SearchDirection, SearchType};

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
const HISTORY_SAVE_MAX: usize = 1024 * 256;

/// Default buffer size for flushing to the history file.
const HISTORY_OUTPUT_BUFFER_SIZE: usize = 64 * 1024;

/// The file access mode we use for creating history files
const HISTORY_FILE_MODE: usize = 0o600;

/// How many times we retry to save
/// Saving may fail if the file is modified in between our opening
/// the file and taking the lock
const max_save_tries: usize = 1024;

/// If the size of `buffer` is at least `min_size`, output the contents `buffer` to `fd`,
/// and clear the string.
fn flush_to_fd(buffer: &mut String, fd: RawFd, min_size: usize) -> std::io::Result<()> {
    if buffer.is_empty() || buffer.len() < min_size {
        return Ok(());
    }

    write_loop(&fd, buffer.as_bytes())?;
    buffer.clear();
    return Ok(());
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
            FLOGF!(
                profile_history,
                "%s: %d.%06d ms",
                self.what,
                duration.as_millis(),
                duration.as_nanos() / 1000
            )
        } else {
            FLOGF!(profile_history, "%s: ??? ms", self.what)
        }
    }
}

/// Returns the path for the history file for the given `session_id`, or `None` if it could not be
/// loaded. If `suffix` is provided, append that suffix to the path; this is used for temporary files.
#[widestrs]
fn history_filename(session_id: &wstr, suffix: &wstr) -> Option<WString> {
    todo!()
    /*
    if session_id.is_empty() {
        return None;
    }

    let Some(result) = path_get_data() else {
        return None;
    };

    result.append("/"L);
    result.append(session_id);
    result.append("_history"L);
    result.append(suffix);
    Some(result)
    */
}

pub type HistoryIdentifier = u64;

#[derive(Clone)]
pub struct HistoryItem {
    contents: WString,
    creation_timestamp: SystemTime,
    required_paths: Vec<WString>,
    identifier: HistoryIdentifier,
    persist_mode: PersistenceMode,
}

impl HistoryItem {
    /// Construct from a text, timestamp, and optional identifier.
    /// If `persist_mode` is `PersistenceMode::Ephemeral`, then do not write this item to disk.
    pub fn new(
        s: WString,
        when: SystemTime,
        ident: HistoryIdentifier,
        persist_mode: PersistenceMode,
    ) -> Self {
        todo!()
    }

    /// Returns the text as a string.
    pub fn str(&self) -> &wstr {
        &self.contents
    }

    fn str_ffi(&self) -> UniquePtr<CxxWString> {
        self.str().to_ffi()
    }

    /// Returns whether the text is empty.
    pub fn empty(&self) -> bool {
        todo!()
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
            SearchType::ContainsGlob => todo!(),
            SearchType::PrefixGlob => todo!(),
            SearchType::ContainsSubsequence => subsequence_in_string(term, &content_to_match),
            SearchType::MatchEverything => true,
            _ => unreachable!("invalid SearchType"),
        }
    }

    fn matches_search_ffi(&self, term: &CxxWString, typ: SearchType, case_sensitive: bool) -> bool {
        self.matches_search(&term.from_ffi(), typ, case_sensitive)
    }

    /// Returns the timestamp for creating this history item.
    pub fn timestamp(&self) -> i64 {
        todo!()
    }

    /// Returns whether this item should be persisted (written to disk).
    pub fn should_write_to_disk(&self) -> bool {
        todo!()
    }

    /// Get the list of arguments which referred to files.
    /// This is used for autosuggestion hinting.
    pub fn get_required_paths(&self) -> &[WString] {
        todo!()
    }

    fn get_required_paths_ffi(&self) -> UniquePtr<wcstring_list_ffi_t> {
        self.get_required_paths().to_ffi()
    }

    /// Set the list of arguments which referred to files.
    /// This is used for autosuggestion hinting.
    pub fn set_required_paths(&mut self, paths: Vec<WString>) {
        todo!()
    }

    fn set_required_paths_ffi(&mut self, paths: &wcstring_list_ffi_t) {
        self.set_required_paths(paths.from_ffi())
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

pub struct History {}

impl History {
    /// Returns history with the given name, creating it if necessary.
    pub fn with_name(name: &wstr) -> Arc<Mutex<Self>> {
        todo!()
    }

    /// Returns whether this is using the default name.
    pub fn is_default(&self) -> bool {
        todo!()
    }

    /// Determines whether the history is empty.
    pub fn is_empty(&self) -> bool {
        todo!()
    }

    /// Remove a history item.
    pub fn remove(&mut self, s: &wstr) {
        todo!()
    }

    /// Remove any trailing ephemeral items.
    pub fn remove_ephemeral_items(&mut self) {
        todo!()
    }

    /// Add a new pending history item to the end, and then begin file detection on the items to
    /// determine which arguments are paths. Arguments may be expanded (e.g. with PWD and variables)
    /// using the given `vars`. The item has the given `persist_mode`.
    pub fn add_pending_with_file_detection(
        &mut self,
        s: &wstr,
        vars: &dyn Environment,
        persist_mode: PersistenceMode,
    ) {
        todo!()
    }

    /// Resolves any pending history items, so that they may be returned in history searches.
    pub fn resolve_pending(&mut self) {
        todo!()
    }

    /// Saves history.
    pub fn save(&mut self) {
        todo!()
    }

    /// Searches history.
    pub fn search(
        &self,
        search_type: SearchType,
        search_args: &[WString],
        show_time_format: &wstr,
        max_items: usize,
        case_sensitive: bool,
        null_terminate: bool,
        reverse: bool,
        cancel_on_signal: bool,
        streams: &mut IoStreams<'_>,
    ) -> bool {
        todo!()
    }

    /// Irreversibly clears history.
    pub fn clear(&mut self) {
        todo!()
    }

    /// Irreversibly clears history for the current session.
    pub fn clear_session(&mut self) {
        todo!()
    }

    /// Populates from older location (in config path, rather than data path).
    pub fn populate_from_config_path(&mut self) {
        todo!()
    }

    /// Populates from a bash history file.
    pub fn populate_from_bash(&mut self, f: &wstr) {
        todo!()
    }

    /// Incorporates the history of other shells into this history.
    pub fn incorporate_external_changes(&mut self) {
        todo!()
    }

    /// Gets all the history into a list. This is intended for the $history environment variable.
    /// This may be long!
    // TODO: remove out param
    pub fn get_history(&self, out: &mut Vec<WString>) {
        todo!()
    }

    /// Let indexes be a list of one-based indexes into the history, matching the interpretation of
    /// `$history`. That is, `$history[1]` is the most recently executed command.
    /// Returns a mapping from index to history item text.
    pub fn items_at_indexes(
        &self,
        indexes: impl Iterator<Item = usize>,
    ) -> HashMap<usize, WString> {
        todo!()
    }

    /// Return the specified history at the specified index. 0 is the index of the current
    /// commandline. (So the most recent item is at index 1.)
    pub fn item_at_index(&self, idx: usize) -> &HistoryItem {
        todo!()
    }

    /// Return the number of history entries.
    pub fn size(&self) -> usize {
        todo!()
    }
}

bitflags! {
    /// Flags for history searching.
    pub struct SearchFlags: u32 {
        /// If set, ignore case.
        const IGNORE_CASE = 1 << 0;
        /// If set, do not deduplicate, which can help performance.
        const NO_DEDUP = 1 << 1;
    }
}

/// Support for searching a history backwards.
/// Note this does NOT de-duplicate; it is the caller's responsibility to do so.
pub struct HistorySearch {}

impl HistorySearch {
    /// Constructs a new history search.
    pub fn new(
        hist: Arc<Mutex<History>>,
        s: &wstr,
        search_type: SearchType,
        flags: SearchFlags,
        starting_index: usize,
    ) -> Self {
        todo!()
    }

    /// Returns the original search term.
    pub fn original_term(&self) -> &wstr {
        todo!()
    }

    fn original_term_ffi(&self) -> UniquePtr<CxxWString> {
        self.original_term().to_ffi()
    }

    /// Finds the next search result. Returns `true` if one was found.
    pub fn go_to_next_match(&mut self, direction: SearchDirection) -> bool {
        todo!()
    }

    /// Returns the current search result item.
    ///
    /// # Panics
    ///
    /// This function panics if there is no current item.
    pub fn current_item(&self) -> &HistoryItem {
        todo!()
    }

    /// Returns the current search result item contents.
    ///
    /// # Panics
    ///
    /// This function panics if there is no current item.
    pub fn current_string(&self) -> &wstr {
        todo!()
    }

    fn current_string_ffi(&self) -> UniquePtr<CxxWString> {
        self.current_string().to_ffi()
    }

    /// Returns the index of the current history item.
    pub fn current_index(&self) -> usize {
        todo!()
    }

    /// Returns whether we are case insensitive.
    pub fn ignores_case(&self) -> bool {
        todo!()
    }
}

/// Saves the new history to disk.
pub fn save_all() {
    todo!()
}

/// Return the prefix for the files to be used for command and read history.
pub fn history_session_id(vars: &dyn Environment) -> WString {
    todo!()
}

/// Given a list of proposed paths and a context, perform variable and home directory expansion,
/// and detect if the result expands to a value which is also the path to a file.
/// Wildcard expansions are suppressed - see implementation comments for why.
///
/// This is used for autosuggestion hinting. If we add an item to history, and one of its arguments
/// refers to a file, then we only want to suggest it if there is a valid file there.
/// This does disk I/O and may only be called in a background thread.
pub fn expand_and_detect_paths(paths: &[&wstr], vars: &dyn Environment) -> Vec<WString> {
    todo!()
}

/// Given a list of proposed paths and a context, expand each one and see if it refers to a file.
/// Wildcard expansions are suppressed.
/// Returns `true` if `paths` is empty or every path is valid.
pub fn all_paths_are_valid(paths: &[WString], ctx: &OperationContext<'_>) -> bool {
    todo!()
}

/// Sets private mode on. Once in private mode, it cannot be turned off.
pub fn start_private_mode(vars: &EnvStack) {
    todo!()
}

/// Queries private mode status.
pub fn in_private_mode(vars: &EnvDyn) -> bool {
    todo!()
}

pub fn in_private_mode_ffi(vars: &EnvDynFFI) -> bool {
    in_private_mode(&vars.0)
}

// ========
// FFI crud
// ========

struct ItemIndexes(HashMap<usize, WString>);

impl ItemIndexes {
    fn get(&self, index: usize) -> UniquePtr<CxxWString> {
        self.0
            .get(&index)
            .map(|s| s.to_ffi())
            .unwrap_or_else(UniquePtr::null)
    }
}

fn rust_history_item_new(
    s: &CxxWString,
    when: i64,
    ident: u64,
    persist_mode: PersistenceMode,
) -> Box<HistoryItem> {
    let s = s.from_ffi();
    let when = if when < 0 {
        UNIX_EPOCH - Duration::from_secs(u64::try_from(-when).unwrap())
    } else {
        UNIX_EPOCH + Duration::from_secs(u64::try_from(when).unwrap())
    };
    Box::new(HistoryItem::new(s, when, ident, persist_mode))
}

struct HistorySharedPtr(Arc<Mutex<History>>);

impl HistorySharedPtr {
    fn inner(&self) -> MutexGuard<History> {
        self.0.lock().expect("Mutex poisoned")
    }

    fn is_default(&self) -> bool {
        self.inner().is_default()
    }
    fn is_empty(&self) -> bool {
        self.inner().is_empty()
    }
    fn remove(&self, s: &CxxWString) {
        self.inner().remove(&s.from_ffi())
    }
    fn remove_ephemeral_items(&self) {
        self.inner().remove_ephemeral_items()
    }
    fn add_pending_with_file_detection(
        &self,
        s: &CxxWString,
        vars: &EnvDynFFI,
        persist_mode: PersistenceMode,
    ) {
        self.inner()
            .add_pending_with_file_detection(&s.from_ffi(), &vars.0, persist_mode)
    }
    fn resolve_pending(&self) {
        self.inner().resolve_pending()
    }
    fn save(&self) {
        self.inner().save()
    }
    fn search(
        &self,
        search_type: SearchType,
        search_args: &wcstring_list_ffi_t,
        show_time_format: &CxxWString,
        max_items: usize,
        case_sensitive: bool,
        null_terminate: bool,
        reverse: bool,
        cancel_on_signal: bool,
        streams: Pin<&mut IoStreams<'_>>,
    ) -> bool {
        let search_args = search_args.from_ffi();
        self.inner().search(
            search_type,
            &search_args,
            &show_time_format.from_ffi(),
            max_items,
            case_sensitive,
            null_terminate,
            reverse,
            cancel_on_signal,
            streams.unpin(),
        )
    }
    fn clear(&self) {
        self.inner().clear()
    }
    fn clear_session(&self) {
        self.inner().clear_session()
    }
    fn populate_from_config_path(&self) {
        self.inner().populate_from_config_path()
    }
    fn populate_from_bash(&self, f: &CxxWString) {
        self.inner().populate_from_bash(&f.from_ffi())
    }
    fn incorporate_external_changes(&self) {
        self.inner().incorporate_external_changes()
    }
    fn get_history(&self) -> UniquePtr<wcstring_list_ffi_t> {
        let mut v = Vec::new();
        self.inner().get_history(&mut v);
        v.to_ffi()
    }
    fn items_at_indexes(&self, indexes: &[isize]) -> Box<ItemIndexes> {
        Box::new(ItemIndexes(self.inner().items_at_indexes(
            indexes.iter().filter_map(|&n| n.try_into().ok()),
        )))
    }
    fn item_at_index(&self, idx: usize) -> Box<HistoryItem> {
        Box::new(self.inner().item_at_index(idx).clone())
    }
    fn size(&self) -> usize {
        self.inner().size()
    }
    fn clone(&self) -> Box<Self> {
        Box::new(Self(Arc::clone(&self.0)))
    }
}

fn history_with_name(name: &CxxWString) -> Box<HistorySharedPtr> {
    Box::new(HistorySharedPtr(History::with_name(&name.from_ffi())))
}

fn rust_history_search_new(
    hist: &HistorySharedPtr,
    s: &CxxWString,
    search_type: SearchType,
    flags: u32,
    starting_index: usize,
) -> Box<HistorySearch> {
    Box::new(HistorySearch::new(
        Arc::clone(&hist.0),
        &s.from_ffi(),
        search_type,
        SearchFlags::from_bits(flags).unwrap(),
        starting_index,
    ))
}

fn rust_session_id(vars: &EnvDynFFI) -> UniquePtr<CxxWString> {
    history_session_id(&vars.0).to_ffi()
}

fn rust_expand_and_detect_paths(
    paths: &wcstring_list_ffi_t,
    vars: &EnvDynFFI,
) -> UniquePtr<wcstring_list_ffi_t> {
    let paths = paths.from_ffi();
    let paths: Vec<&wstr> = paths.iter().map(|s| s.as_utfstr()).collect();
    expand_and_detect_paths(&paths, &vars.0).to_ffi()
}

fn rust_all_paths_are_valid(paths: &wcstring_list_ffi_t, ctx: &OperationContext<'_>) -> bool {
    let paths = paths.from_ffi();
    all_paths_are_valid(&paths, ctx)
}

fn start_private_mode_ffi(vars: &EnvStackRefFFI) {
    start_private_mode(&vars.0)
}

fn history_never_mmap() -> bool {
    todo!()
}
