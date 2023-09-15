use crate::signal::Signal;
use crate::wchar::{widestrs, wstr, WString};
use crate::wcstringutil::join_strings;
use bitflags::bitflags;
use lazy_static::lazy_static;
use libc::c_int;
use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::Arc;

/// The character used to delimit path and non-path variables in exporting and in string expansion.
pub const PATH_ARRAY_SEP: char = ':';
pub const NONPATH_ARRAY_SEP: char = ' ';

bitflags! {
    /// Flags that may be passed as the 'mode' in env_stack_t::set() / environment_t::get().
    /// The default is empty.
    #[repr(C)]
    #[derive(Copy, Clone, Default, PartialEq, Eq)]
    pub struct EnvMode: u16 {
        /// Flag for local (to the current block) variable.
        const LOCAL = 1 << 0;
        const FUNCTION = 1 << 1;
        /// Flag for global variable.
        const GLOBAL = 1 << 2;
        /// Flag for universal variable.
        const UNIVERSAL = 1 << 3;
        /// Flag for exported (to commands) variable.
        const EXPORT = 1 << 4;
        /// Flag for unexported variable.
        const UNEXPORT = 1 << 5;
        /// Flag to mark a variable as a path variable.
        const PATHVAR = 1 << 6;
        /// Flag to unmark a variable as a path variable.
        const UNPATHVAR = 1 << 7;
        /// Flag for variable update request from the user. All variable changes that are made directly
        /// by the user, such as those from the `read` and `set` builtin must have this flag set. It
        /// serves one purpose: to indicate that an error should be returned if the user is attempting
        /// to modify a var that should not be modified by direct user action; e.g., a read-only var.
        const USER = 1 << 8;
    }
}

impl From<EnvMode> for autocxx::c_int {
    fn from(val: EnvMode) -> Self {
        autocxx::c_int(i32::from(val.bits()))
    }
}
impl From<EnvMode> for u16 {
    fn from(val: EnvMode) -> Self {
        val.bits()
    }
}

/// Return values for `env_stack_t::set()`.
pub mod status {
    pub const ENV_OK: i32 = 0;
    pub const ENV_PERM: i32 = 1;
    pub const ENV_SCOPE: i32 = 2;
    pub const ENV_INVALID: i32 = 3;
    pub const ENV_NOT_FOUND: i32 = 4;
}

/// Return values for `EnvStack::set()`.
pub enum EnvStackSetResult {
    ENV_OK,
    ENV_PERM,
    ENV_SCOPE,
    ENV_INVALID,
    ENV_NOT_FOUND,
}

impl Default for EnvStackSetResult {
    fn default() -> Self {
        EnvStackSetResult::ENV_OK
    }
}

/// A struct of configuration directories, determined in main() that fish will optionally pass to
/// env_init.
#[derive(Default)]
pub struct ConfigPaths {
    pub data: PathBuf,    // e.g., /usr/local/share
    pub sysconf: PathBuf, // e.g., /usr/local/etc
    pub doc: PathBuf,     // e.g., /usr/local/share/doc/fish
    pub bin: PathBuf,     // e.g., /usr/local/bin
}

/// A collection of status and pipestatus.
#[derive(Clone, Debug)]
pub struct Statuses {
    /// Status of the last job to exit.
    pub status: c_int,

    /// Signal from the most recent process in the last job that was terminated by a signal.
    /// None if all processes exited normally.
    pub kill_signal: Option<Signal>,

    /// Pipestatus value.
    pub pipestatus: Vec<c_int>,
}

impl Statuses {
    /// Return a Statuses for a single process status.
    pub fn just(status: c_int) -> Self {
        Statuses {
            status,
            kill_signal: None,
            pipestatus: vec![status],
        }
    }
}

impl Default for Statuses {
    fn default() -> Self {
        Self::just(0)
    }
}

bitflags! {
    #[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
    pub struct EnvVarFlags: u8 {
        const EXPORT = 1 << 0;    // whether the variable is exported
        const READ_ONLY = 1 << 1; // whether the variable is read only
        const PATHVAR = 1 << 2;   // whether the variable is a path variable
    }
}

// A shared, empty list.
lazy_static! {
    static ref EMPTY_LIST: Arc<Box<[WString]>> = Arc::new(Box::new([]));
}

/// EnvVar is an immutable value-type data structure representing the value of an environment
/// variable.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EnvVar {
    /// The list of values in this variable.
    /// Arc allows for cheap copying
    values: Arc<Box<[WString]>>,
    /// The variable's flags.
    flags: EnvVarFlags,
}

impl Default for EnvVar {
    fn default() -> Self {
        EnvVar {
            values: EMPTY_LIST.clone(),
            flags: EnvVarFlags::empty(),
        }
    }
}

impl EnvVar {
    /// Creates a new `EnvVar`.
    pub fn new(value: WString, flags: EnvVarFlags) -> Self {
        Self::new_vec(vec![value], flags)
    }

    /// Creates a new `EnvVar`.
    pub fn new_vec(values: Vec<WString>, flags: EnvVarFlags) -> Self {
        EnvVar {
            values: Arc::new(values.into_boxed_slice()),
            flags,
        }
    }

    /// Creates a new `EnvVar`, inferring the flags from the variable name.
    pub fn new_from_name_vec(name: &wstr, values: Vec<WString>) -> Self {
        Self::new_vec(values, Self::flags_for(name))
    }

    /// Creates a new `EnvVar`, inferring the flags from the variable name.
    pub fn new_from_name(name: &wstr, value: WString) -> Self {
        Self::new_from_name_vec(name, vec![value])
    }

    /// Returns whether the variable has no values or a single empty value.
    pub fn is_empty(&self) -> bool {
        self.values.is_empty() || (self.values.len() == 1 && self.values[0].is_empty())
    }

    /// Returns whether the variable is exported.
    pub fn exports(&self) -> bool {
        self.flags.contains(EnvVarFlags::EXPORT)
    }

    /// Returns whether the variable is a path variable.
    pub fn is_pathvar(&self) -> bool {
        self.flags.contains(EnvVarFlags::PATHVAR)
    }

    /// Returns whether the variable is read-only.
    pub fn is_read_only(&self) -> bool {
        self.flags.contains(EnvVarFlags::READ_ONLY)
    }

    /// Returns the variable's flags.
    pub fn get_flags(&self) -> EnvVarFlags {
        self.flags
    }

    /// Returns the variable's value as a string.
    pub fn as_string(&self) -> WString {
        join_strings(&self.values, self.get_delimiter())
    }

    /// Copies the variable's values into an existing list, avoiding reallocation if possible.
    pub fn to_list(&self, out: &mut Vec<WString>) {
        // Try to avoid reallocation as much as possible.
        out.resize(self.values.len(), WString::new());
        for (i, val) in self.values.iter().enumerate() {
            out[i].clone_from(val);
        }
    }

    /// Returns the variable's values.
    pub fn as_list(&self) -> &[WString] {
        &self.values
    }

    /// Returns the delimiter character used when converting from a list to a string.
    pub fn get_delimiter(&self) -> char {
        if self.is_pathvar() {
            PATH_ARRAY_SEP
        } else {
            NONPATH_ARRAY_SEP
        }
    }

    /// Returns a copy of the variable with new values.
    pub fn setting_vals(&mut self, values: Vec<WString>) -> Self {
        EnvVar {
            values: Arc::new(values.into_boxed_slice()),
            flags: self.flags,
        }
    }

    /// Returns a copy of the variable with the export flag changed.
    pub fn setting_exports(&mut self, export: bool) -> Self {
        let mut flags = self.flags;
        flags.set(EnvVarFlags::EXPORT, export);
        EnvVar {
            values: self.values.clone(),
            flags,
        }
    }

    /// Returns a copy of the variable with the path variable flag changed.
    pub fn setting_pathvar(&mut self, pathvar: bool) -> Self {
        let mut flags = self.flags;
        flags.set(EnvVarFlags::PATHVAR, pathvar);
        EnvVar {
            values: self.values.clone(),
            flags,
        }
    }

    /// Returns flags for a variable with the given name.
    pub fn flags_for(name: &wstr) -> EnvVarFlags {
        let mut result = EnvVarFlags::empty();
        if is_read_only(name) {
            result.insert(EnvVarFlags::READ_ONLY);
        }
        result
    }
}

pub type VarTable = HashMap<WString, EnvVar>;

mod electric {
    pub(super) const READONLY: u8 = 1 << 0; // May not be modified by the user.
    pub(super) const COMPUTED: u8 = 1 << 1; // Value is dynamically computed.
    pub(super) const EXPORTS: u8 = 1 << 2; // Exported to child processes.
    pub(super) type ElectricVarFlags = u8;
}

pub struct ElectricVar {
    pub name: &'static wstr,
    flags: electric::ElectricVarFlags,
}

// Keep sorted alphabetically
#[rustfmt::skip]
#[widestrs]
pub const ELECTRIC_VARIABLES: &[ElectricVar] = &[
    ElectricVar{name: "FISH_VERSION"L, flags: electric::READONLY},
    ElectricVar{name: "PWD"L, flags: electric::READONLY | electric::COMPUTED | electric::EXPORTS},
    ElectricVar{name: "SHLVL"L, flags: electric::READONLY | electric::EXPORTS},
    ElectricVar{name: "_"L, flags: electric::READONLY},
    ElectricVar{name: "fish_kill_signal"L, flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: "fish_killring"L, flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: "fish_pid"L, flags:electric::READONLY},
    ElectricVar{name: "history"L, flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: "hostname"L, flags:electric::READONLY},
    ElectricVar{name: "pipestatus"L, flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: "status"L, flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: "status_generation"L, flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: "umask"L, flags:electric::COMPUTED},
    ElectricVar{name: "version"L, flags:electric::READONLY},
];
assert_sorted_by_name!(ELECTRIC_VARIABLES);

impl ElectricVar {
    /// \return the ElectricVar with the given name, if any
    pub fn for_name(name: &wstr) -> Option<&'static ElectricVar> {
        match ELECTRIC_VARIABLES.binary_search_by(|ev| ev.name.cmp(name)) {
            Ok(idx) => Some(&ELECTRIC_VARIABLES[idx]),
            Err(_) => None,
        }
    }

    pub fn readonly(&self) -> bool {
        self.flags & electric::READONLY != 0
    }

    pub fn computed(&self) -> bool {
        self.flags & electric::COMPUTED != 0
    }

    pub fn exports(&self) -> bool {
        self.flags & electric::EXPORTS != 0
    }

    /// Supports assert_sorted_by_name.
    fn as_char_slice(&self) -> &[char] {
        self.name.as_char_slice()
    }
}

/// Check if a variable may not be set using the set command.
pub fn is_read_only(name: &wstr) -> bool {
    if let Some(ev) = ElectricVar::for_name(name) {
        ev.flags & electric::READONLY != 0
    } else {
        false
    }
}
