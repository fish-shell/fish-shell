//! The classes responsible for autoloading functions and completions.

use crate::common::{escape, ScopeGuard};
use crate::env::Environment;
use crate::io::IoChain;
use crate::parser::Parser;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::wchar::{wstr, WString, L};
use crate::wutil::{file_id_for_path, FileId, INVALID_FILE_ID};
use lru::LruCache;
use std::collections::{HashMap, HashSet};
use std::num::NonZeroUsize;
use std::time;

/// autoload_t is a class that knows how to autoload .fish files from a list of directories. This
/// is used by autoloading functions and completions. It maintains a file cache, which is
/// responsible for potentially cached accesses of files, and then a list of files that have
/// actually been autoloaded. A client may request a file to autoload given a command name, and may
/// be returned a path which it is expected to source.
/// autoload_t does not have any internal locks; it is the responsibility of the caller to lock
/// it.
#[derive(Default)]
pub struct Autoload {
    /// The environment variable whose paths we observe.
    env_var_name: &'static wstr,

    /// A map from command to the files we have autoloaded.
    autoloaded_files: HashMap<WString, FileId>,

    /// The list of commands that we are currently autoloading.
    current_autoloading: HashSet<WString>,

    /// The autoload cache.
    /// This is a unique_ptr because want to change it if the value of our environment variable
    /// changes. This is never null (but it may be a cache with no paths).
    cache: Box<AutoloadFileCache>,
}

impl Autoload {
    /// Construct an autoloader that loads from the paths given by \p env_var_name.
    pub fn new(env_var_name: &'static wstr) -> Self {
        Self {
            env_var_name,
            ..Default::default()
        }
    }

    /// Given a command, get a path to autoload.
    /// For example, if the environment variable is 'fish_function_path' and the command is 'foo',
    /// this will look for a file 'foo.fish' in one of the directories given by fish_function_path.
    /// If there is no such file, OR if the file has been previously resolved and is now unchanged,
    /// this will return none. But if the file is either new or changed, this will return the path.
    /// After returning a path, the command is marked in-progress until the caller calls
    /// mark_autoload_finished() with the same command. Note this does not actually execute any
    /// code; it is the caller's responsibility to load the file.
    pub fn resolve_command(&mut self, cmd: &wstr, env: &dyn Environment) -> Option<WString> {
        if let Some(var) = env.get(self.env_var_name) {
            self.resolve_command_impl(cmd, var.as_list())
        } else {
            self.resolve_command_impl(cmd, &[])
        }
    }

    /// Helper to actually perform an autoload.
    /// This is a static function because it executes fish script, and so must be called without
    /// holding any particular locks.
    pub fn perform_autoload(path: &wstr, parser: &Parser) {
        // We do the useful part of what exec_subshell does ourselves
        // - we source the file.
        // We don't create a buffer or check ifs or create a read_limit

        let script_source = L!("source ").to_owned() + &escape(path)[..];
        let prev_statuses = parser.get_last_statuses();
        let _put_back = ScopeGuard::new((), |()| parser.set_last_statuses(prev_statuses));
        parser.eval(&script_source, &IoChain::new());
    }

    /// Mark that a command previously returned from path_to_autoload is finished autoloading.
    pub fn mark_autoload_finished(&mut self, cmd: &wstr) {
        let removed = self.current_autoloading.remove(cmd);
        assert!(removed, "cmd was not being autoloaded");
    }

    /// \return whether a command is currently being autoloaded.
    pub fn autoload_in_progress(&self, cmd: &wstr) -> bool {
        self.current_autoloading.contains(cmd)
    }

    /// \return whether a command could potentially be autoloaded.
    /// This does not actually mark the command as being autoloaded.
    pub fn can_autoload(&mut self, cmd: &wstr) -> bool {
        self.cache.check(cmd, true /* allow stale */).is_some()
    }

    /// \return whether autoloading has been attempted for a command.
    pub fn has_attempted_autoload(&self, cmd: &wstr) -> bool {
        self.cache.is_cached(cmd)
    }

    /// \return the names of all commands that have been autoloaded. Note this includes "in-flight"
    /// commands.
    pub fn get_autoloaded_commands(&self) -> Vec<WString> {
        let mut result = Vec::with_capacity(self.autoloaded_files.len());
        for k in self.autoloaded_files.keys() {
            result.push(k.to_owned());
        }
        // Sort the output to make it easier to test.
        result.sort();
        result
    }

    /// Mark that all autoloaded files have been forgotten.
    /// Future calls to path_to_autoload() will return previously-returned paths.
    pub fn clear(&mut self) {
        // Note there is no reason to invalidate the cache here.
        self.autoloaded_files.clear();
    }

    /// Invalidate any underlying cache.
    /// This is exposed for testing.
    #[cfg(test)]
    fn invalidate_cache(&mut self) {
        self.cache = Box::new(AutoloadFileCache::with_dirs(self.cache.dirs().to_owned()));
    }

    /// Like resolve_autoload(), but accepts the paths directly.
    /// This is exposed for testing.
    fn resolve_command_impl(&mut self, cmd: &wstr, paths: &[WString]) -> Option<WString> {
        // Are we currently in the process of autoloading this?
        if self.current_autoloading.contains(cmd) {
            return None;
        }

        // Check to see if our paths have changed. If so, replace our cache.
        // Note we don't have to modify autoloadable_files_. We'll naturally detect if those have
        // changed when we query the cache.
        if paths != self.cache.dirs() {
            self.cache = Box::new(AutoloadFileCache::with_dirs(paths.to_owned()));
        }

        // Do we have an entry to load?
        let Some(file) = self.cache.check(cmd, false) else {
            return None;
        };

        // Is this file the same as what we previously autoloaded?
        if let Some(loaded_file) = self.autoloaded_files.get(cmd) {
            if *loaded_file == file.file_id {
                // The file has been autoloaded and is unchanged.
                return None;
            }
        }

        // We're going to (tell our caller to) autoload this command.
        self.current_autoloading.insert(cmd.to_owned());
        self.autoloaded_files.insert(cmd.to_owned(), file.file_id);
        Some(file.path)
    }
}

/// The time before we'll recheck an autoloaded file.
const AUTOLOAD_STALENESS_INTERVALL: u64 = 15;

/// Represents a file that we might want to autoload.
#[derive(Clone)]
struct AutoloadableFile {
    /// The path to the file.
    path: WString,
    /// The metadata for the file.
    file_id: FileId,
}

// A timestamp is a monotonic point in time.
type Timestamp = time::Instant;
type MissesLruCache = LruCache<WString, Timestamp>;

struct KnownFile {
    file: AutoloadableFile,
    last_checked: Timestamp,
}

/// Class representing a cache of files that may be autoloaded.
/// This is responsible for performing cached accesses to a set of paths.
struct AutoloadFileCache {
    /// The directories from which to load.
    dirs: Vec<WString>,

    /// Our LRU cache of checks that were misses.
    /// The key is the command, the  value is the time of the check.
    misses_cache: MissesLruCache,

    /// The set of files that we have returned to the caller, along with the time of the check.
    /// The key is the command (not the path).
    known_files: HashMap<WString, KnownFile>,
}

impl Default for AutoloadFileCache {
    fn default() -> Self {
        Self::new()
    }
}

impl AutoloadFileCache {
    /// Initialize with a set of directories.
    fn with_dirs(dirs: Vec<WString>) -> Self {
        Self {
            dirs,
            misses_cache: MissesLruCache::new(NonZeroUsize::new(1024).unwrap()),
            known_files: HashMap::new(),
        }
    }

    /// Initialize with empty directories.
    fn new() -> Self {
        Self::with_dirs(vec![])
    }

    /// \return the directories.
    fn dirs(&self) -> &[WString] {
        &self.dirs
    }

    /// Check if a command \p cmd can be loaded.
    /// If \p allow_stale is true, allow stale entries; otherwise discard them.
    /// This returns an autoloadable file, or none() if there is no such file.
    fn check(&mut self, cmd: &wstr, allow_stale: bool) -> Option<AutoloadableFile> {
        // Check hits.
        if let Some(value) = self.known_files.get(cmd) {
            if allow_stale || Self::is_fresh(value.last_checked, Self::current_timestamp()) {
                // Re-use this cached hit.
                return Some(value.file.clone());
            }
            // The file is stale, remove it.
            self.known_files.remove(cmd);
        }

        // Check misses.
        if let Some(miss) = self.misses_cache.get(cmd) {
            if allow_stale || Self::is_fresh(*miss, Self::current_timestamp()) {
                // Re-use this cached miss.
                return None;
            }
            // The miss is stale, remove it.
            self.misses_cache.pop(cmd);
        }

        // We couldn't satisfy this request from the cache. Hit the disk.
        let file = self.locate_file(cmd);
        if let Some(file) = file.as_ref() {
            let old_value = self.known_files.insert(
                cmd.to_owned(),
                KnownFile {
                    file: file.clone(),
                    last_checked: Self::current_timestamp(),
                },
            );
            assert!(
                old_value.is_none(),
                "Known files cache should not have contained this cmd"
            );
        } else {
            let old_value = self
                .misses_cache
                .put(cmd.to_owned(), Self::current_timestamp());
            assert!(
                old_value.is_none(),
                "Misses cache should not have contained this cmd",
            );
        }
        file
    }

    /// \return true if a command is cached (either as a hit or miss).
    fn is_cached(&self, cmd: &wstr) -> bool {
        self.known_files.contains_key(cmd) || self.misses_cache.contains(cmd)
    }

    /// \return the current timestamp.
    fn current_timestamp() -> Timestamp {
        Timestamp::now()
    }

    /// \return whether a timestamp is fresh enough to use.
    fn is_fresh(then: Timestamp, now: Timestamp) -> bool {
        let seconds = now.duration_since(then).as_secs();
        seconds < AUTOLOAD_STALENESS_INTERVALL
    }

    /// Attempt to find an autoloadable file by searching our path list for a given command.
    /// \return the file, or none() if none.
    fn locate_file(&self, cmd: &wstr) -> Option<AutoloadableFile> {
        // If the command is empty or starts with NULL (i.e. is empty as a path)
        // we'd try to source the *directory*, which exists.
        // So instead ignore these here.
        if cmd.is_empty() {
            return None;
        }
        if cmd.as_char_slice()[0] == '\0' {
            return None;
        }
        // Re-use the storage for path.
        let mut path;
        for dir in self.dirs() {
            // Construct the path as dir/cmd.fish
            path = dir.to_owned();
            path.push('/');
            path.push_utfstr(cmd);
            path.push_str(".fish");

            let file_id = file_id_for_path(&path);
            if file_id != INVALID_FILE_ID {
                // Found it.
                return Some(AutoloadableFile { path, file_id });
            }
        }
        None
    }
}

#[test]
#[serial]
fn test_autoload() {
    test_init();
    use crate::common::{charptr2wcstring, wcs2zstring, write_loop};
    use crate::fds::wopen_cloexec;
    use crate::wutil::sprintf;
    use libc::{O_CREAT, O_RDWR};

    macro_rules! run {
        ( $fmt:expr $(, $arg:expr )* $(,)? ) => {
             let cmd = wcs2zstring(&sprintf!($fmt $(, $arg)*));
             let status = unsafe { libc::system(cmd.as_ptr()) };
             assert!(status == 0);
        };
    }

    fn touch_file(path: &wstr) {
        let fd = wopen_cloexec(path, O_RDWR | O_CREAT, 0o666).unwrap();
        write_loop(&fd, "Hello".as_bytes()).unwrap();
        unsafe { libc::close(fd) };
    }

    let mut t1 = "/tmp/fish_test_autoload.XXXXXX\0".as_bytes().to_vec();
    let p1 = charptr2wcstring(unsafe { libc::mkdtemp(t1.as_mut_ptr().cast()) });
    let mut t2 = "/tmp/fish_test_autoload.XXXXXX\0".as_bytes().to_vec();
    let p2 = charptr2wcstring(unsafe { libc::mkdtemp(t2.as_mut_ptr().cast()) });

    let paths = &[p1.clone(), p2.clone()];
    let mut autoload = Autoload::new(L!("test_var"));
    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_none());
    assert!(autoload
        .resolve_command_impl(L!("nothing"), paths)
        .is_none());
    assert!(autoload.get_autoloaded_commands().is_empty());

    run!("touch %ls/file1.fish", p1);
    run!("touch %ls/file2.fish", p2);
    autoload.invalidate_cache();

    assert!(!autoload.autoload_in_progress(L!("file1")));
    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_some());
    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_none());
    assert!(autoload.autoload_in_progress(L!("file1")));
    assert!(autoload.get_autoloaded_commands() == vec![L!("file1")]);
    autoload.mark_autoload_finished(L!("file1"));
    assert!(!autoload.autoload_in_progress(L!("file1")));
    assert!(autoload.get_autoloaded_commands() == vec![L!("file1")]);

    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_none());
    assert!(autoload
        .resolve_command_impl(L!("nothing"), paths)
        .is_none());
    assert!(autoload.resolve_command_impl(L!("file2"), paths).is_some());
    assert!(autoload.resolve_command_impl(L!("file2"), paths).is_none());
    autoload.mark_autoload_finished(L!("file2"));
    assert!(autoload.resolve_command_impl(L!("file2"), paths).is_none());
    assert!((autoload.get_autoloaded_commands() == vec![L!("file1"), L!("file2")]));

    autoload.clear();
    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_some());
    autoload.mark_autoload_finished(L!("file1"));
    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_none());
    assert!(autoload
        .resolve_command_impl(L!("nothing"), paths)
        .is_none());
    assert!(autoload.resolve_command_impl(L!("file2"), paths).is_some());
    assert!(autoload.resolve_command_impl(L!("file2"), paths).is_none());
    autoload.mark_autoload_finished(L!("file2"));

    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_none());
    touch_file(&sprintf!("%ls/file1.fish", p1));
    autoload.invalidate_cache();
    assert!(autoload.resolve_command_impl(L!("file1"), paths).is_some());
    autoload.mark_autoload_finished(L!("file1"));

    run!(L!("rm -Rf %ls"), p1);
    run!(L!("rm -Rf %ls"), p2);
}
