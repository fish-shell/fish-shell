use crate::common::{cstr2wcstring, wcs2zstring};
use crate::env::{EnvVar, EnvVarFlags, VarTable};
use crate::fds::AutoCloseFd;
use crate::path::{path_get_config_remoteness, DirRemoteness};
use crate::wchar::{wstr, WString};
use crate::wutil::{FileId, INVALID_FILE_ID};
use std::collections::hash_map::Entry;
use std::collections::HashSet;
use std::ffi::CString;
use std::os::fd::RawFd;

/// Callback data, reflecting a change in universal variables.
pub struct CallbackData {
    // The name of the variable.
    pub key: WString,

    // The value of the variable, or none if it is erased.
    pub val: Option<EnvVar>,
}
impl CallbackData {
    /// Construct from a key and maybe a value.
    pub fn new(key: WString, val: Option<EnvVar>) -> Self {
        Self { key, val }
    }
    /// \return whether this callback represents an erased variable.
    pub fn is_erase(&self) -> bool {
        self.val.is_none()
    }
}

pub type CallbackDataList = Vec<CallbackData>;

// List of fish universal variable formats.
// This is exposed for testing.
pub enum UvarFormat {
    fish_2_x,
    fish_3_0,
    future,
}

/// Function to get an identifier based on the hostname.
pub fn get_hostname_identifier() -> Option<WString> {
    /// Maximum length of hostname. Longer hostnames are truncated.
    const HOSTNAME_LEN: usize = 255;

    // The behavior of gethostname if the buffer size is insufficient differs by implementation and
    // libc version Work around this by using a "guaranteed" sufficient buffer size then truncating
    // the result.
    let mut hostname = [b'\0'; 256];
    if unsafe {
        libc::gethostname(
            std::ptr::addr_of_mut!(hostname).cast(),
            std::mem::size_of_val(&hostname),
        )
    } == 0
    {
        let mut result = cstr2wcstring(&hostname);
        result.truncate(HOSTNAME_LEN);
        // Don't return an empty hostname, we may attempt to open a directory instead.
        if !result.is_empty() {
            return Some(result);
        }
    }
    None
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
    // \return flags from the variable with the given name.
    pub fn get_flags(&self, name: &wstr) -> Option<EnvVarFlags> {
        self.vars.get(name).map(|var| var.get_flags())
    }
    // Sets a variable.
    pub fn set(&mut self, key: &wstr, var: &EnvVar) {
        match self.vars.entry(key.to_owned()) {
            Entry::Occupied(mut entry) => {
                if entry.get() == var {
                    return;
                }
                entry.insert(var.clone());
            }
            Entry::Vacant(mut entry) => {
                entry.insert(var.clone());
            }
        };
        self.modified.insert(key.to_owned());
        if var.exports() {
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
        self.narrow_vars_path = wcs2zstring(&self.vars_path);

        self.load_from_path(callbacks);
        // Successfully loaded from our normal path.
    }

    /// Reads and writes variables at the correct path. Returns true if modified variables were
    /// written.
    pub fn sync(&mut self, callbacks: &mut CallbackDataList) -> bool {
        todo!()
    }

    /// Populate a variable table \p out_vars from a \p s string.
    /// This is exposed for testing only.
    /// \return the format of the file that we read.
    pub fn populate_variables(s: &[u8], out_vars: &mut VarTable) -> UvarFormat {
        todo!()
    }

    /// Guess a file format. Exposed for testing only.
    pub fn format_for_contents(s: &[u8]) -> UvarFormat {
        todo!()
    }

    /// Serialize a variable list. Exposed for testing only.
    pub fn serialize_with_vars(vars: &VarTable) -> Vec<u8> {
        todo!()
    }

    /// Exposed for testing only.
    pub fn is_ok_to_save(&self) -> bool {
        self.ok_to_save
    }

    /// Access the export generation.
    pub fn get_export_generation(&self) -> u64 {
        self.export_generation
    }

    /// \return whether we are initialized.
    fn initialized(&self) -> bool {
        !self.vars_path.is_empty()
    }

    fn load_from_path(&mut self, callbacks: &mut CallbackDataList) -> bool {
        // self.narrow_vars_path
        todo!()
    }

    fn load_from_fd(&mut self, fd: RawFd, callbacks: &mut CallbackDataList) {
        todo!()
    }

    // Functions concerned with saving.
    fn open_and_acquire_lock(&mut self, path: &wstr) -> Option<AutoCloseFd> {
        todo!()
    }
    fn open_temporary_file(&mut self, directory: &wstr) -> Option<WString> {
        todo!()
    }
    fn write_to_fd(&self, fd: RawFd, path: &wstr) -> bool {
        todo!()
    }
    fn move_new_vars_file_into_place(&mut self, src: &wstr, dst: &wstr) -> bool {
        todo!()
    }

    // Given a variable table, generate callbacks representing the difference between our vars and
    // the new vars. Also update our exports generation count as necessary.
    fn generate_callbacks_and_update_exports(
        &mut self,
        new_vars: &VarTable,
        callbacks: &mut CallbackDataList,
    ) {
        todo!()
    }

    // Given a variable table, copy unmodified values into self.
    fn acquire_variables(&mut self, vars_to_acquire: VarTable) {
        todo!()
    }

    fn populate_1_variable(
        input: &wstr,
        flags: EnvVarFlags,
        vars: &mut VarTable,
        storage: &mut WString,
    ) -> bool {
        todo!()
    }
    fn parse_message_2x_internal(msg: &wstr, vars: &mut VarTable, storage: &mut WString) {
        todo!()
    }
    fn parse_message_30_internal(msg: &wstr, vars: &mut VarTable, storage: &mut WString) {
        todo!()
    }
    fn read_message_internal(fd: RawFd, vars: &mut VarTable) {
        todo!()
    }
    /// \return the default variable path, or an empty string on failure.
    fn save(directory: &wstr, vars_path: &wstr) -> bool {
        todo!()
    }
}

fn default_vars_path() -> WString {
    todo!()
}

pub enum NotifierStrategy {
    // Poll on shared memory.
    strategy_shmem_polling,

    // Mac-specific notify(3) implementation.
    strategy_notifyd,

    // Strategy that uses a named pipe. Somewhat complex, but portable and doesn't require
    // polling most of the time.
    strategy_named_pipe,
}

/// The "universal notifier" is an object responsible for broadcasting and receiving universal
/// variable change notifications. These notifications do not contain the change, but merely
/// indicate that the uvar file has changed. It is up to the uvar subsystem to re-read the file.
///
/// We support a few notification strategies. Not all strategies are supported on all platforms.
///
/// Notifiers may request polling, and/or provide a file descriptor to be watched for readability in
/// select().
///
/// To request polling, the notifier overrides usec_delay_between_polls() to return a positive
/// value. That value will be used as the timeout in select(). When select returns, the loop invokes
/// poll(). poll() should return true to indicate that the file may have changed.
///
/// To provide a file descriptor, the notifier overrides notification_fd() to return a non-negative
/// fd. This will be added to the "read" file descriptor list in select(). If the fd is readable,
/// notification_fd_became_readable() will be called; that function should be overridden to return
/// true if the file may have changed.
pub trait UniversalNotifier {
    // Does a fast poll(). Returns true if changed.
    fn poll(&self) -> bool;

    // Triggers a notification.
    fn post_notification(&self);

    // Recommended delay between polls. A value of 0 means no polling required (so no timeout).
    fn usec_delay_between_polls(&self) -> u64;

    // Returns the fd from which to watch for events, or -1 if none.
    fn notification_fd(&self) -> RawFd;

    // The notification_fd is readable; drain it. Returns true if a notification is considered to
    // have been posted.
    fn notification_fd_became_readable(&self, fd: RawFd) -> bool;
}

fn resolve_default_strategy() -> NotifierStrategy {
    todo!()
}

// Default instance. Other instances are possible for testing.
fn default_notifier() -> &'static dyn UniversalNotifier {
    todo!()
}

/// Factory constructor.
fn new_notifier_for_strategy(strat: NotifierStrategy, test_path: Option<&wstr>) {
    todo!();
}

pub fn get_runtime_path() -> WString {
    todo!()
}
