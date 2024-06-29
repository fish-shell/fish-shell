#![allow(clippy::bad_bit_mask)]

use crate::common::{
    read_loop, str2wcstring, timef, unescape_string, valid_var_name, wcs2zstring, write_loop,
    UnescapeFlags, UnescapeStringStyle,
};
use crate::env::{EnvVar, EnvVarFlags, VarTable};
use crate::fallback::fish_mkstemp_cloexec;
use crate::fds::{open_cloexec, wopen_cloexec};
use crate::flog::{FLOG, FLOGF};
use crate::path::path_get_config;
use crate::path::{path_get_config_remoteness, DirRemoteness};
use crate::wchar::{decode_byte_from_char, prelude::*};
use crate::wcstringutil::{join_strings, string_suffixes_string, LineIterator};
use crate::wutil::{
    file_id_for_fd, file_id_for_path, file_id_for_path_narrow, wdirname, wrealpath, wrename, wstat,
    wunlink, FileId, INVALID_FILE_ID,
};
use errno::{errno, Errno};
use libc::{EINTR, LOCK_EX};
use nix::{fcntl::OFlag, sys::stat::Mode};
use std::collections::hash_map::Entry;
use std::collections::HashSet;
use std::ffi::CString;
use std::fs::File;
use std::mem::MaybeUninit;
use std::os::fd::{AsFd, AsRawFd, RawFd};
use std::os::unix::prelude::MetadataExt;

// Pull in the O_EXLOCK constant if it is defined, otherwise set it to 0.
#[cfg(any(bsd, target_os = "macos"))]
const O_EXLOCK: OFlag = OFlag::O_EXLOCK;

#[cfg(not(any(bsd, target_os = "macos")))]
const O_EXLOCK: OFlag = OFlag::empty();

/// Callback data, reflecting a change in universal variables.
pub struct CallbackData {
    // The name of the variable.
    pub key: WString,

    // The value of the variable, or none if it is erased.
    pub val: Option<EnvVar>,
}

pub type CallbackDataList = Vec<CallbackData>;

// List of fish universal variable formats.
// This is exposed for testing.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum UvarFormat {
    fish_2_x,
    fish_3_0,
    future,
}

/// Class representing universal variables.
pub struct EnvUniversal {
    // Path that we save to. This is set in initialize(). If empty, initialize has not been called.
    vars_path: WString,
    narrow_vars_path: CString,

    // The table of variables.
    vars: VarTable,

    // Keys that have been modified, and need to be written. A value here that is not present in
    // vars indicates a deleted value.
    modified: HashSet<WString>,

    // A generation count which is incremented every time an exported variable is modified.
    export_generation: u64,

    // Whether it's OK to save. This may be set to false if we discover that a future version of
    // fish wrote the uvars contents.
    ok_to_save: bool,

    // If true, attempt to flock the uvars file.
    // This latches to false if the file is found to be remote, where flock may hang.
    do_flock: bool,

    // File id from which we last read.
    last_read_file: FileId,
}

impl EnvUniversal {
    // Construct an empty universal variables.
    pub fn new() -> Self {
        Self {
            vars_path: Default::default(),
            narrow_vars_path: Default::default(),
            vars: Default::default(),
            modified: Default::default(),
            export_generation: 1,
            ok_to_save: true,
            do_flock: true,
            last_read_file: INVALID_FILE_ID,
        }
    }
    // Get the value of the variable with the specified name.
    pub fn get(&self, name: &wstr) -> Option<EnvVar> {
        self.vars.get(name).cloned()
    }
    // Return flags from the variable with the given name.
    pub fn get_flags(&self, name: &wstr) -> Option<EnvVarFlags> {
        self.vars.get(name).map(|var| var.get_flags())
    }
    // Sets a variable.
    pub fn set(&mut self, key: &wstr, var: EnvVar) {
        let exports = var.exports();
        match self.vars.entry(key.to_owned()) {
            Entry::Occupied(mut entry) => {
                if entry.get() == &var {
                    return;
                }
                entry.insert(var);
            }
            Entry::Vacant(entry) => {
                entry.insert(var);
            }
        };
        self.modified.insert(key.to_owned());
        if exports {
            self.export_generation += 1;
        }
    }
    // Removes a variable. Returns true if it was found, false if not.
    pub fn remove(&mut self, key: &wstr) -> bool {
        if let Some(var) = self.vars.remove(key) {
            if var.exports() {
                self.export_generation += 1;
            }
            self.modified.insert(key.to_owned());
            return true;
        }
        false
    }

    // Gets variable names.
    pub fn get_names(&self, show_exported: bool, show_unexported: bool) -> Vec<WString> {
        let mut result = vec![];
        for (key, var) in &self.vars {
            if (var.exports() && show_exported) || (!var.exports() && show_unexported) {
                result.push(key.clone());
            }
        }
        result
    }

    /// Get a view on the universal variable table.
    pub fn get_table(&self) -> &VarTable {
        &self.vars
    }

    /// Initialize this uvars for the default path.
    /// This should be called at most once on any given instance.
    pub fn initialize(&mut self, callbacks: &mut CallbackDataList) {
        // Set do_flock to false immediately if the default variable path is on a remote filesystem.
        // See #7968.
        if path_get_config_remoteness() == DirRemoteness::remote {
            self.do_flock = false;
        }
        self.initialize_at_path(callbacks, default_vars_path());
    }

    /// Initialize a this uvars for a given path.
    /// This is exposed for testing only.
    pub fn initialize_at_path(&mut self, callbacks: &mut CallbackDataList, path: WString) {
        if path.is_empty() {
            return;
        }
        assert!(!self.initialized(), "Already initialized");
        self.vars_path = path;

        if self.load_from_path(callbacks) {
            // Successfully loaded from our normal path.
        }
    }

    /// Reads and writes variables at the correct path. Returns true if modified variables were
    /// written.
    pub fn sync(&mut self, callbacks: &mut CallbackDataList) -> bool {
        if !self.initialized() {
            return false;
        }

        FLOG!(uvar_file, "universal log sync");
        // Our saving strategy:
        //
        // 1. Open the file, producing an fd.
        // 2. Lock the file (may be combined with step 1 on systems with O_EXLOCK)
        // 3. After taking the lock, check if the file at the given path is different from what we
        // opened. If so, start over.
        // 4. Read from the file. This can be elided if its dev/inode is unchanged since the last read
        // 5. Open an adjacent temporary file
        // 6. Write our changes to an adjacent file
        // 7. Move the adjacent file into place via rename. This is assumed to be atomic.
        // 8. Release the lock and close the file
        //
        // Consider what happens if Process 1 and 2 both do this simultaneously. Can there be data loss?
        // Process 1 opens the file and then attempts to take the lock. Now, either process 1 will see
        // the original file, or process 2's new file. If it sees the new file, we're OK: it's going to
        // read from the new file, and so there's no data loss. If it sees the old file, then process 2
        // must have locked it (if process 1 locks it, switch their roles). The lock will block until
        // process 2 reaches step 7; at that point process 1 will reach step 2, notice that the file has
        // changed, and then start over.
        //
        // It's possible that the underlying filesystem does not support locks (lockless NFS). In this
        // case, we risk data loss if two shells try to write their universal variables simultaneously.
        // In practice this is unlikely, since uvars are usually written interactively.
        //
        // Prior versions of fish used a hard link scheme to support file locking on lockless NFS. The
        // risk here is that if the process crashes or is killed while holding the lock, future
        // instances of fish will not be able to obtain it. This seems to be a greater risk than that of
        // data loss on lockless NFS. Users who put their home directory on lockless NFS are playing
        // with fire anyways.
        // If we have no changes, just load.
        if self.modified.is_empty() {
            self.load_from_path_narrow(callbacks);
            FLOG!(uvar_file, "universal log no modifications");
            return false;
        }

        let directory = wdirname(&self.vars_path).to_owned();

        FLOG!(uvar_file, "universal log performing full sync");

        // Open the file.
        let Some(mut vars_file) = self.open_and_acquire_lock() else {
            FLOG!(uvar_file, "universal log open_and_acquire_lock() failed");
            return false;
        };

        // Read from it.
        self.load_from_fd(&mut vars_file, callbacks);

        if self.ok_to_save {
            self.save(&directory)
        } else {
            true
        }
    }

    /// Populate a variable table `out_vars` from a `s` string.
    /// This is exposed for testing only.
    /// Return the format of the file that we read.
    pub fn populate_variables(s: &[u8], out_vars: &mut VarTable) -> UvarFormat {
        // Decide on the format.
        let format = Self::format_for_contents(s);

        let iter = LineIterator::new(s);
        let mut wide_line = WString::new();
        let mut storage = WString::new();
        for line in iter {
            // Skip empties and constants.
            if line.is_empty() || line[0] == b'#' {
                continue;
            }

            // Convert to UTF8.
            wide_line.clear();
            let Ok(line) = std::str::from_utf8(line) else {
                continue;
            };
            wide_line = WString::from_str(line);

            match format {
                UvarFormat::fish_2_x => {
                    Self::parse_message_2x_internal(&wide_line, out_vars, &mut storage);
                }
                UvarFormat::fish_3_0 => {
                    Self::parse_message_30_internal(&wide_line, out_vars, &mut storage);
                }
                // For future formats, just try with the most recent one.
                UvarFormat::future => {
                    Self::parse_message_30_internal(&wide_line, out_vars, &mut storage);
                }
            }
        }
        format
    }

    /// Guess a file format. Exposed for testing only.
    /// Return the format corresponding to file contents `s`.
    pub fn format_for_contents(s: &[u8]) -> UvarFormat {
        // Walk over leading comments, looking for one like '# version'
        let iter = LineIterator::new(s);
        for line in iter {
            if line.is_empty() {
                continue;
            }
            if line[0] != b'#' {
                // Exhausted leading comments.
                break;
            }
            // Note scanf %s is max characters to write; add 1 for null terminator.
            let mut versionbuf: MaybeUninit<[u8; 64 + 1]> = MaybeUninit::uninit();
            // Safety: test-only
            let cstr = CString::new(line).unwrap();
            if unsafe {
                libc::sscanf(
                    cstr.as_ptr(),
                    b"# VERSION: %64s\0".as_ptr().cast(),
                    versionbuf.as_mut_ptr(),
                )
            } != 1
            {
                continue;
            }

            // Try reading the version.
            let versionbuf = unsafe { versionbuf.assume_init() };
            return if versionbuf.starts_with(UVARS_VERSION_3_0)
                && versionbuf[UVARS_VERSION_3_0.len()] == b'\0'
            {
                UvarFormat::fish_3_0
            } else {
                UvarFormat::future
            };
        }
        // No version found, assume 2.x
        return UvarFormat::fish_2_x;
    }

    /// Serialize a variable list. Exposed for testing only.
    pub fn serialize_with_vars(vars: &VarTable) -> Vec<u8> {
        let mut contents = vec![];
        contents.extend_from_slice(SAVE_MSG);
        contents.extend_from_slice(b"# VERSION: ");
        contents.extend_from_slice(UVARS_VERSION_3_0);
        contents.push(b'\n');

        // Preserve legacy behavior by sorting the values first
        let mut cloned: Vec<(&wstr, &EnvVar)> =
            vars.iter().map(|(key, var)| (key.as_ref(), var)).collect();
        cloned.sort_by(|(lkey, _), (rkey, _)| lkey.cmp(rkey));

        for (key, var) in cloned {
            // Append the entry. Note that append_file_entry may fail, but that only affects one
            // variable; soldier on.
            append_file_entry(
                var.get_flags(),
                key,
                &encode_serialized(var.as_list()),
                &mut contents,
            );
        }
        contents
    }

    /// Exposed for testing only.
    #[cfg(test)]
    pub fn is_ok_to_save(&self) -> bool {
        self.ok_to_save
    }

    /// Access the export generation.
    pub fn get_export_generation(&self) -> u64 {
        self.export_generation
    }

    /// Return whether we are initialized.
    fn initialized(&self) -> bool {
        !self.vars_path.is_empty()
    }

    fn load_from_path(&mut self, callbacks: &mut CallbackDataList) -> bool {
        self.narrow_vars_path = wcs2zstring(&self.vars_path);
        self.load_from_path_narrow(callbacks)
    }

    fn load_from_path_narrow(&mut self, callbacks: &mut CallbackDataList) -> bool {
        // Check to see if the file is unchanged. We do this again in load_from_fd, but this avoids
        // opening the file unnecessarily.
        if self.last_read_file != INVALID_FILE_ID
            && file_id_for_path_narrow(&self.narrow_vars_path) == self.last_read_file
        {
            FLOG!(uvar_file, "universal log sync elided based on fast stat()");
            return true;
        }

        let Ok(mut file) = open_cloexec(&self.narrow_vars_path, OFlag::O_RDONLY, Mode::empty())
        else {
            return false;
        };

        FLOG!(uvar_file, "universal log reading from file");
        self.load_from_fd(&mut file, callbacks);
        true
    }

    // Load environment variables from the opened [`File`] `file`. It must be mutable because we
    // will read from the underlying fd.
    fn load_from_fd(&mut self, file: &mut File, callbacks: &mut CallbackDataList) {
        // Get the dev / inode.
        let current_file = file_id_for_fd(file.as_fd());
        if current_file == self.last_read_file {
            FLOG!(uvar_file, "universal log sync elided based on fstat()");
        } else {
            // Read a variables table from the file.
            let mut new_vars = VarTable::new();
            let format = Self::read_message_internal(file.as_raw_fd(), &mut new_vars);

            // Hacky: if the read format is in the future, avoid overwriting the file: never try to
            // save.
            if format == UvarFormat::future {
                self.ok_to_save = false;
            }

            // Announce changes and update our exports generation.
            self.generate_callbacks_and_update_exports(&new_vars, callbacks);

            // Acquire the new variables.
            self.acquire_variables(new_vars);
            self.last_read_file = current_file;
        }
    }

    // Functions concerned with saving.
    fn open_and_acquire_lock(&mut self) -> Option<File> {
        // Attempt to open the file for reading at the given path, atomically acquiring a lock. On BSD,
        // we can use O_EXLOCK. On Linux, we open the file, take a lock, and then compare fstat() to
        // stat(); if they match, it means that the file was not replaced before we acquired the lock.
        //
        // We pass O_RDONLY with O_CREAT; this creates a potentially empty file. We do this so that we
        // have something to lock on.
        let mut locked_by_open = false;
        let mut flags = OFlag::O_RDWR | OFlag::O_CREAT;

        if !O_EXLOCK.is_empty() && self.do_flock {
            flags |= O_EXLOCK;
            locked_by_open = true;
        }

        loop {
            let mut file =
                match wopen_cloexec(&self.vars_path, flags, Mode::from_bits_truncate(0o644)) {
                    Ok(file) => file,
                    Err(nix::Error::EINTR) => continue,
                    Err(err) => {
                        if !O_EXLOCK.is_empty() {
                            if flags.contains(O_EXLOCK)
                                && [nix::Error::ENOTSUP, nix::Error::EOPNOTSUPP].contains(&err)
                            {
                                // Filesystem probably does not support locking. Give up on locking.
                                // Note that on Linux the two errno symbols have the same value but on BSD they're
                                // different.
                                flags &= !O_EXLOCK;
                                self.do_flock = false;
                                locked_by_open = false;
                                continue;
                            }
                        }
                        FLOG!(
                            error,
                            wgettext_fmt!(
                                "Unable to open universal variable file '%s': %s",
                                &self.vars_path,
                                err.to_string()
                            )
                        );
                        return None;
                    }
                };

            // Lock if we want to lock and open() didn't do it for us.
            // If flock fails, give up on locking forever.
            if self.do_flock && !locked_by_open {
                if !flock_uvar_file(&mut file) {
                    self.do_flock = false;
                }
            }

            // Hopefully we got the lock. However, it's possible the file changed out from under us
            // while we were waiting for the lock. Make sure that didn't happen.
            if file_id_for_fd(file.as_fd()) != file_id_for_path(&self.vars_path) {
                // Oops, it changed! Try again.
                drop(file);
            } else {
                return Some(file);
            }
        }
    }

    fn open_temporary_file(
        &mut self,
        directory: &wstr,
        out_path: &mut WString,
    ) -> Result<File, Errno> {
        // Create and open a temporary file for writing within the given directory. Try to create a
        // temporary file, up to 10 times. We don't use mkstemps because we want to open it CLO_EXEC.
        // This should almost always succeed on the first try.
        assert!(!string_suffixes_string(L!("/"), directory));

        let mut attempt = 0;
        let tmp_name_template = directory.to_owned() + L!("/fishd.tmp.XXXXXX");
        let result = loop {
            attempt += 1;
            let result = fish_mkstemp_cloexec(wcs2zstring(&tmp_name_template));
            match (result, attempt) {
                (Ok(r), _) => break r,
                (Err(e), 10) => {
                    FLOG!(
                        error,
                        // We previously used to log a copy of the buffer we expected mk(o)stemp to
                        // update with the new path, but mkstemp(3) says the contents of the buffer
                        // are undefined in case of EEXIST, but left unchanged in case of EINVAL. So
                        // just log the original template we pass in to the function instead.
                        wgettext_fmt!(
                            "Unable to create temporary file '%ls': %s",
                            &tmp_name_template,
                            e.to_string()
                        )
                    );
                    return Err(e);
                }
                _ => continue,
            }
        };

        *out_path = str2wcstring(result.1.as_bytes());
        Ok(result.0)
    }

    /// Writes our state to the fd. path is provided only for error reporting.
    fn write_to_fd(&mut self, fd: impl AsFd, path: &wstr) -> std::io::Result<usize> {
        let fd = fd.as_fd();
        let contents = Self::serialize_with_vars(&self.vars);

        let res = write_loop(&fd, &contents);
        match res.as_ref() {
            Ok(_) => {
                // Since we just wrote out this file, it matches our internal state; pretend we read from it.
                self.last_read_file = file_id_for_fd(fd);
            }
            Err(err) => {
                let error = Errno(err.raw_os_error().unwrap());
                FLOG!(
                    error,
                    wgettext_fmt!(
                        "Unable to write to universal variables file '%ls': %s",
                        path,
                        error.to_string()
                    ),
                );
            }
        }

        // We don't close the file.
        res
    }

    fn move_new_vars_file_into_place(&mut self, src: &wstr, dst: &wstr) -> bool {
        let ret = wrename(src, dst);
        if ret != 0 {
            let error = errno();
            FLOG!(
                error,
                wgettext_fmt!(
                    "Unable to rename file from '%ls' to '%ls': %s",
                    src,
                    dst,
                    error.to_string()
                )
            );
        }
        ret == 0
    }

    // Given a variable table, generate callbacks representing the difference between our vars and
    // the new vars. Also update our exports generation count as necessary.
    fn generate_callbacks_and_update_exports(
        &mut self,
        new_vars: &VarTable,
        callbacks: &mut CallbackDataList,
    ) {
        // Construct callbacks for erased values.
        for (key, value) in &self.vars {
            // Skip modified values.
            if self.modified.contains(key) {
                continue;
            }

            // If the value is not present in new_vars, it has been erased.
            if !new_vars.contains_key(key) {
                callbacks.push(CallbackData {
                    key: key.clone(),
                    val: None,
                });
                if value.exports() {
                    self.export_generation += 1;
                }
            }
        }

        // Construct callbacks for newly inserted or changed values.
        for (key, new_entry) in new_vars {
            // Skip modified values.
            if self.modified.contains(key) {
                continue;
            }

            let existing = self.vars.get(key);

            // See if the value has changed.
            let old_exports = existing.map_or(false, |v| v.exports());
            let export_changed = old_exports != new_entry.exports();
            let value_changed = existing.map_or(false, |v| v != new_entry);
            if export_changed || value_changed {
                self.export_generation += 1;
            }
            if existing.is_none() || export_changed || value_changed {
                // Value is set for the first time, or has changed.
                callbacks.push(CallbackData {
                    key: key.clone(),
                    val: Some(new_entry.clone()),
                });
            }
        }
    }

    // Given a variable table, copy unmodified values into self.
    fn acquire_variables(&mut self, mut vars_to_acquire: VarTable) {
        // Copy modified values from existing vars to vars_to_acquire.
        for key in &self.modified {
            match self.vars.get(key) {
                None => {
                    /* The value has been deleted. */
                    vars_to_acquire.remove(key);
                }
                Some(src) => {
                    // The value has been modified. Copy it over. Note we can destructively modify the
                    // source entry in vars since we are about to get rid of this->vars entirely.
                    vars_to_acquire.insert(key.clone(), src.clone());
                }
            }
        }

        // We have constructed all the callbacks and updated vars_to_acquire. Acquire it!
        self.vars = vars_to_acquire;
    }

    fn populate_1_variable(
        input: &wstr,
        flags: EnvVarFlags,
        vars: &mut VarTable,
        storage: &mut WString,
    ) -> bool {
        let s = skip_spaces(input);
        let Some(colon) = s.chars().position(|c| c == ':') else {
            return false;
        };

        // Parse out the value into storage, and decode it into a variable.
        storage.clear();
        let Some(unescaped) = unescape_string(
            &s[colon + 1..],
            UnescapeStringStyle::Script(UnescapeFlags::default()),
        ) else {
            return false;
        };
        *storage = unescaped;
        let var = EnvVar::new_vec(decode_serialized(&*storage), flags);

        // Parse out the key and write into the map.
        *storage = s[..colon].to_owned();
        let key = &*storage;
        (*vars).insert(key.clone(), var);
        true
    }
    /// Parse message msg per fish 3.0 format.
    fn parse_message_30_internal(msg: &wstr, vars: &mut VarTable, storage: &mut WString) {
        use fish3_uvars as f3;
        if msg.starts_with(L!("#")) {
            return;
        }

        let mut cursor = msg;
        if !r#match(&mut cursor, f3::SETUVAR) {
            FLOGF!(warning, PARSE_ERR, msg);
            return;
        }
        // Parse out flags.
        let mut flags = EnvVarFlags::default();
        loop {
            cursor = skip_spaces(cursor);
            if cursor.char_at(0) != '-' {
                break;
            }
            if r#match(&mut cursor, f3::EXPORT) {
                flags |= EnvVarFlags::EXPORT;
            } else if r#match(&mut cursor, f3::PATH) {
                flags |= EnvVarFlags::PATHVAR;
            } else {
                // Skip this unknown flag, for future proofing.
                while !cursor.is_empty() && !matches!(cursor.char_at(0), ' ' | '\t') {
                    cursor = &cursor[1..];
                }
            }
        }

        // Populate the variable with these flags.
        if !Self::populate_1_variable(cursor, flags, vars, storage) {
            FLOGF!(warning, PARSE_ERR, msg);
        }
    }

    /// Parse message msg per fish 2.x format.
    fn parse_message_2x_internal(msg: &wstr, vars: &mut VarTable, storage: &mut WString) {
        use fish2x_uvars as f2x;
        let mut cursor = msg;

        if cursor.char_at(0) == '#' {
            return;
        }
        let mut flags = EnvVarFlags::default();
        if r#match(&mut cursor, f2x::SET_EXPORT) {
            flags |= EnvVarFlags::EXPORT;
        } else if r#match(&mut cursor, f2x::SET) {
        } else {
            FLOGF!(warning, PARSE_ERR, msg);
            return;
        }

        if !Self::populate_1_variable(cursor, flags, vars, storage) {
            FLOGF!(warning, PARSE_ERR, msg);
        }
    }

    fn read_message_internal(fd: RawFd, vars: &mut VarTable) -> UvarFormat {
        // Read everything from the fd. Put a sane limit on it.
        let mut contents = vec![];
        let mut buffer = [0_u8; 4096];
        while contents.len() < MAX_READ_SIZE {
            match read_loop(&fd, &mut buffer) {
                Ok(0) | Err(_) => break,
                Ok(amt) => contents.extend_from_slice(&buffer[..amt]),
            }
        }

        // Handle overlong files.
        if contents.len() > MAX_READ_SIZE {
            contents.truncate(MAX_READ_SIZE);
            // Back up to a newline.
            let newline = contents.iter().rposition(|c| *c == b'\n').unwrap_or(0);
            contents.truncate(newline);
        }

        Self::populate_variables(&contents, vars)
    }

    // Write our file contents.
    // Return true on success, false on failure.
    fn save(&mut self, directory: &wstr) -> bool {
        use crate::common::ScopeGuard;
        assert!(self.ok_to_save, "It's not OK to save");

        // Open adjacent temporary file.
        let mut private_file_path = WString::new();
        let Ok(private_fd) = self.open_temporary_file(directory, &mut private_file_path) else {
            return false;
        };
        // unlink pfp upon failure. In case of success, it (already) won't exist.
        let delete_pfp = ScopeGuard::new(private_file_path, |path| {
            wunlink(path);
        });
        let private_file_path = &delete_pfp;

        // Write to it.
        if self.write_to_fd(&private_fd, private_file_path).is_err() {
            FLOG!(uvar_file, "universal log write_to_fd() failed");
            return false;
        }

        let real_path = wrealpath(&self.vars_path).unwrap_or_else(|| self.vars_path.clone());

        // Ensure we maintain ownership and permissions (#2176).
        // let mut sbuf : libc::stat = MaybeUninit::uninit();
        if let Ok(md) = wstat(&real_path) {
            if unsafe { libc::fchown(private_fd.as_raw_fd(), md.uid(), md.gid()) } == -1 {
                FLOG!(uvar_file, "universal log fchown() failed");
            }
            #[allow(clippy::useless_conversion)]
            let mode: libc::mode_t = md.mode().try_into().unwrap();
            if unsafe { libc::fchmod(private_fd.as_raw_fd(), mode) } == -1 {
                FLOG!(uvar_file, "universal log fchmod() failed");
            }
        }

        // Linux by default stores the mtime with low precision, low enough that updates that occur
        // in quick succession may result in the same mtime (even the nanoseconds field). So
        // manually set the mtime of the new file to a high-precision clock. Note that this is only
        // necessary because Linux aggressively reuses inodes, causing the ABA problem; on other
        // platforms we tend to notice the file has changed due to a different inode (or file size!)
        //
        // The current time within the Linux kernel is cached, and generally only updated on a timer
        // interrupt. So if the timer interrupt is running at 10 milliseconds, the cached time will
        // only be updated once every 10 milliseconds.
        //
        // It's probably worth finding a simpler solution to this. The tests ran into this, but it's
        // unlikely to affect users.
        #[cfg(any(target_os = "linux", target_os = "android"))]
        {
            let mut times: [libc::timespec; 2] = unsafe { std::mem::zeroed() };
            times[0].tv_nsec = libc::UTIME_OMIT; // don't change ctime
            if unsafe { libc::clock_gettime(libc::CLOCK_REALTIME, &mut times[1]) } != 0 {
                unsafe {
                    libc::futimens(private_fd.as_raw_fd(), &times[0]);
                }
            }
        }

        // Apply new file.
        if !self.move_new_vars_file_into_place(private_file_path, &real_path) {
            FLOG!(
                uvar_file,
                "universal log move_new_vars_file_into_place() failed"
            );
            return false;
        }

        // Success at last. All of our modified variables have now been written out.
        self.modified.clear();
        ScopeGuard::cancel(delete_pfp);
        true
    }
}

/// Return the default variable path, or an empty string on failure.
pub fn default_vars_path() -> WString {
    if let Some(mut path) = default_vars_path_directory() {
        path.push_str("/fish_variables");
        return path;
    }
    WString::new()
}

/// Error message.
const PARSE_ERR: &wstr = L!("Unable to parse universal variable message: '%ls'");

/// Small note about not editing ~/.fishd manually. Inserted at the top of all .fishd files.
const SAVE_MSG: &[u8] = b"# This file contains fish universal variable definitions.\n";

/// Version for fish 3.0
const UVARS_VERSION_3_0: &[u8] = b"3.0";

// Maximum file size we'll read.
const MAX_READ_SIZE: usize = 16 * 1024 * 1024;

// Fields used in fish 2.x uvars.

mod fish2x_uvars {
    pub const SET: &[u8] = b"SET";
    pub const SET_EXPORT: &[u8] = b"SET_EXPORT";
}
// Fields used in fish 3.0 uvars
mod fish3_uvars {
    pub const SETUVAR: &[u8] = b"SETUVAR";
    pub const EXPORT: &[u8] = b"--export";
    pub const PATH: &[u8] = b"--path";
}

/// Return the default variable path, or an empty string on failure.
fn default_vars_path_directory() -> Option<WString> {
    path_get_config()
}

/// Test if the message msg contains the command cmd.
/// On success, updates the cursor to just past the command.
fn r#match(inout_cursor: &mut &wstr, cmd: &[u8]) -> bool {
    let cursor = *inout_cursor;
    if !cmd
        .iter()
        .copied()
        .map(char::from)
        .eq(cursor.chars().take(cmd.len()))
    {
        return false;
    }
    let len = cmd.len();
    if cursor.len() != len && !matches!(cursor.char_at(len), ' ' | '\t') {
        return false;
    }
    *inout_cursor = &cursor[len..];
    true
}

/// The universal variable format has some funny escaping requirements; here we try to be safe.
fn is_universal_safe_to_encode_directly(c: char) -> bool {
    if !(32..=128).contains(&u32::from(c)) {
        return false;
    }

    c.is_alphanumeric() || matches!(c, '/' | '_')
}

/// Escape specified string.
fn full_escape(input: &wstr) -> WString {
    let mut out = WString::new();
    for c in input.chars() {
        if is_universal_safe_to_encode_directly(c) {
            out.push(c);
        } else if c.is_ascii() {
            sprintf!(=> &mut out, "\\x%.2x", u32::from(c));
        } else if let Some(encoded_byte) = decode_byte_from_char(c) {
            sprintf!(=> &mut out, "\\x%.2x", u32::from(encoded_byte));
        } else if u32::from(c) < 65536 {
            sprintf!(=> &mut out, "\\u%.4x", u32::from(c));
        } else {
            sprintf!(=> &mut out, "\\U%.8x", u32::from(c));
        }
    }
    out
}

/// Converts input to UTF-8 and appends it to receiver.
fn append_utf8(input: &wstr, receiver: &mut Vec<u8>) {
    // Notably we convert between wide and narrow strings without decoding our private-use
    // characters.
    receiver.extend_from_slice(input.to_string().as_bytes());
}

/// Creates a file entry like "SET fish_color_cwd:FF0". Appends the result to *result (as UTF8).
/// Returns true on success. storage may be used for temporary storage, to avoid allocations.
fn append_file_entry(
    flags: EnvVarFlags,
    key_in: &wstr,
    val_in: &wstr,
    result: &mut Vec<u8>,
) -> bool {
    use fish3_uvars as f3;

    // Record the length on entry, in case we need to back up.
    let mut success = true;
    let result_length_on_entry = result.len();

    // Append SETVAR header.
    result.extend_from_slice(f3::SETUVAR);
    result.push(b' ');

    // Append flags.
    if flags.contains(EnvVarFlags::EXPORT) {
        result.extend_from_slice(f3::EXPORT);
        result.push(b' ');
    }
    if flags.contains(EnvVarFlags::PATHVAR) {
        result.extend_from_slice(f3::PATH);
        result.push(b' ');
    }

    // Append variable name like "fish_color_cwd".
    if !valid_var_name(key_in) {
        FLOGF!(error, "Illegal variable name: '%ls'", key_in);
        success = false;
    }
    if success {
        append_utf8(key_in, result);
    }

    // Append ":".
    if success {
        result.push(b':');
    }

    // Append value.
    if success {
        append_utf8(&full_escape(val_in), result);
    }

    // Append newline.
    if success {
        result.push(b'\n');
    }

    // Don't modify result on failure. It's sufficient to simply resize it since all we ever did was
    // append to it.
    if !success {
        result.truncate(result_length_on_entry);
    }

    success
}

/// Encoding of a null string.
const ENV_NULL: &wstr = L!("\x1d");

/// Character used to separate arrays in universal variables file.
/// This is 30, the ASCII record separator.
const UVAR_ARRAY_SEP: char = '\x1e';

/// Decode a serialized universal variable value into a list.
fn decode_serialized(val: &wstr) -> Vec<WString> {
    if val == ENV_NULL {
        return vec![];
    }
    val.split(UVAR_ARRAY_SEP).map(|v| v.to_owned()).collect()
}

/// Decode a a list into a serialized universal variable value.
fn encode_serialized(vals: &[WString]) -> WString {
    if vals.is_empty() {
        return ENV_NULL.to_owned();
    }
    join_strings(vals, UVAR_ARRAY_SEP)
}

/// Try locking the file.
/// Return true on success, false on error.
fn flock_uvar_file(file: &mut File) -> bool {
    let start_time = timef();
    while unsafe { libc::flock(file.as_raw_fd(), LOCK_EX) } == -1 {
        if errno().0 != EINTR {
            return false; // do nothing per issue #2149
        }
    }
    let duration = timef() - start_time;
    if duration > 0.25 {
        FLOG!(
            warning,
            wgettext_fmt!(
                "Locking the universal var file took too long (%.3f seconds).",
                duration
            )
        );
        return false;
    }
    true
}

fn skip_spaces(mut s: &wstr) -> &wstr {
    while s.starts_with(L!(" ")) || s.starts_with(L!("\t")) {
        s = &s[1..];
    }
    s
}
