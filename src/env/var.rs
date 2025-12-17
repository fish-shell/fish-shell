use crate::signal::Signal;
use crate::wcstringutil::join_strings;
use bitflags::bitflags;
use fish_wchar::{L, WString, wstr};
use libc::c_int;
use std::collections::HashMap;
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

impl From<EnvMode> for u16 {
    fn from(val: EnvMode) -> Self {
        val.bits()
    }
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

/// EnvVar is an immutable value-type data structure representing the value of an environment
/// variable.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EnvVar {
    /// The list of values in this variable.
    /// Arc allows for cheap copying
    values: Arc<[WString]>,
    /// The variable's flags.
    flags: EnvVarFlags,
}

impl Default for EnvVar {
    fn default() -> Self {
        use std::sync::OnceLock;
        /// A shared read-only empty list.
        static EMPTY_LIST: OnceLock<Arc<[WString]>> = OnceLock::new();
        let empty_list = EMPTY_LIST.get_or_init(|| Arc::new([]));

        EnvVar {
            values: Arc::clone(empty_list),
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
            values: values.into(),
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
    pub fn setting_vals(&self, values: Vec<WString>) -> Self {
        EnvVar {
            values: values.into(),
            flags: self.flags,
        }
    }

    /// Returns a copy of the variable with the export flag changed.
    pub fn setting_exports(&self, export: bool) -> Self {
        let mut flags = self.flags;
        flags.set(EnvVarFlags::EXPORT, export);
        EnvVar {
            values: self.values.clone(),
            flags,
        }
    }

    /// Returns a copy of the variable with the path variable flag changed.
    pub fn setting_pathvar(&self, pathvar: bool) -> Self {
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

pub const FISH_TERMINAL_COLOR_THEME_VAR: &wstr = L!("fish_terminal_color_theme");

// Keep sorted alphabetically
#[rustfmt::skip]
pub const ELECTRIC_VARIABLES: &[ElectricVar] = &[
    ElectricVar{name: L!("FISH_VERSION"), flags: electric::READONLY},
    ElectricVar{name: L!("PWD"), flags: electric::READONLY | electric::COMPUTED | electric::EXPORTS},
    ElectricVar{name: L!("SHLVL"), flags: electric::READONLY | electric::EXPORTS},
    ElectricVar{name: L!("_"), flags: electric::READONLY},
    ElectricVar{name: L!("fish_kill_signal"), flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: L!("fish_killring"), flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: L!("fish_pid"), flags:electric::READONLY},
    ElectricVar{name: FISH_TERMINAL_COLOR_THEME_VAR, flags:electric::READONLY},
    ElectricVar{name: L!("history"), flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: L!("hostname"), flags:electric::READONLY},
    ElectricVar{name: L!("pipestatus"), flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: L!("status"), flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: L!("status_generation"), flags:electric::READONLY | electric::COMPUTED},
    ElectricVar{name: L!("umask"), flags:electric::COMPUTED},
    ElectricVar{name: L!("version"), flags:electric::READONLY},
];
assert_sorted_by_name!(ELECTRIC_VARIABLES);

impl ElectricVar {
    /// Return the ElectricVar with the given name, if any
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
}

/// Check if a variable may not be set using the set command.
pub fn is_read_only(name: &wstr) -> bool {
    if let Some(ev) = ElectricVar::for_name(name) {
        ev.flags & electric::READONLY != 0
    } else {
        false
    }
}

#[cfg(test)]
mod tests {
    use super::{EnvMode, EnvVar, EnvVarFlags};
    use crate::env::environment::{EnvStack, Environment};
    use crate::tests::prelude::*;
    use crate::wchar::prelude::*;
    use std::{
        mem::MaybeUninit,
        time::{SystemTime, UNIX_EPOCH},
    };

    /// Helper for test_timezone_env_vars().
    fn return_timezone_hour(tstamp: SystemTime, timezone: &wstr) -> libc::c_int {
        let vars = EnvStack::globals().create_child(true /* dispatches_var_changes */);

        vars.set_one(L!("TZ"), EnvMode::EXPORT, timezone.to_owned());

        let _var = vars.get(L!("TZ"));

        #[allow(deprecated)]
        let tstamp: libc::time_t = tstamp
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs()
            .try_into()
            .unwrap();
        let mut local_time = MaybeUninit::uninit();
        unsafe { libc::localtime_r(&tstamp, local_time.as_mut_ptr()) };
        let local_time = unsafe { local_time.assume_init() };
        local_time.tm_hour
    }

    /// Verify that setting TZ calls tzset() in the current shell process.
    fn test_timezone_env_vars() {
        // Confirm changing the timezone affects fish's idea of the local time.
        let tstamp = SystemTime::now();

        let first_tstamp = return_timezone_hour(tstamp, L!("UTC-1"));
        let second_tstamp = return_timezone_hour(tstamp, L!("UTC-2"));
        let delta = second_tstamp - first_tstamp;
        assert!(delta == 1 || delta == -23);
    }

    // Verify that setting special env vars have the expected effect on the current shell process.
    #[test]
    #[serial]
    fn test_env_vars() {
        let _cleanup = test_init();
        test_timezone_env_vars();
        // TODO: Add tests for the locale vars.

        let v1 = EnvVar::new(L!("abc").to_owned(), EnvVarFlags::EXPORT);
        let v2 = EnvVar::new_vec(vec![L!("abc").to_owned()], EnvVarFlags::EXPORT);
        let v3 = EnvVar::new_vec(vec![L!("abc").to_owned()], EnvVarFlags::empty());
        let v4 = EnvVar::new_vec(
            vec![L!("abc").to_owned(), L!("def").to_owned()],
            EnvVarFlags::EXPORT,
        );
        assert_eq!(v1, v2);
        assert_ne!(v1, v3);
        assert_ne!(v1, v4);
    }
}
