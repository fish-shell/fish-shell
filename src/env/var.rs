use crate::env::r#impl::is_read_only;
use crate::signal::RawSignal;
use fish_wcstringutil::join_strings;
use fish_widestring::{L, WString, wstr};
use libc::c_int;
use std::collections::HashMap;
use std::sync::Arc;

/// The character used to delimit path and non-path variables in exporting and in string expansion.
pub const PATH_ARRAY_SEP: char = ':';
pub const NONPATH_ARRAY_SEP: char = ' ';

/// Options passed to environment get and set operations.
#[derive(Copy, Clone, PartialEq)]
pub struct EnvMode {
    /// Flag for local (to the current block) variable.
    pub local: bool,
    pub function: bool,
    /// Flag for global variable.
    pub global: bool,
    /// Flag for universal variable.
    pub universal: bool,
    /// Whether to export or unexport this variable.
    pub export: Option<bool>,
    /// Whether to mark/unmark this variable as a path variable.
    pub pathvar: Option<bool>,
}

impl Default for EnvMode {
    fn default() -> Self {
        Self::DEFAULT
    }
}

impl EnvMode {
    const DEFAULT: Self = Self {
        local: false,
        function: false,
        global: false,
        universal: false,
        export: None,
        pathvar: None,
    };

    pub const EXPORTED: Self = Self {
        export: Some(true),
        ..Self::DEFAULT
    };

    pub const UNEXPORT: Self = Self {
        export: Some(false),
        ..Self::DEFAULT
    };

    pub const LOCAL: Self = Self {
        local: true,
        ..Self::DEFAULT
    };

    pub const LOCAL_EXPORTED: Self = Self {
        export: Some(true),
        ..Self::LOCAL
    };

    pub const GLOBAL: Self = Self {
        global: true,
        ..Self::DEFAULT
    };

    pub const GLOBAL_EXPORTED: Self = Self {
        export: Some(true),
        ..Self::GLOBAL
    };

    pub const UNIVERSAL: Self = Self {
        universal: true,
        ..Self::DEFAULT
    };
}

#[derive(Copy, Clone, Default)]
pub struct EnvSetMode {
    pub mode: EnvMode,

    /// Flag for variable update request from the user. All variable changes that are made directly
    /// by the user, such as those from the `read` and `set` builtin must have this flag set. It
    /// serves to indicate that an error should be returned if the user is attempting to modify
    /// a var that should not be modified by direct user action; e.g., a read-only var.
    pub user: bool,

    pub is_repainting: bool,
}

impl EnvSetMode {
    pub fn new(mode: EnvMode, is_repainting: bool) -> Self {
        Self::new_with(mode, false, is_repainting)
    }
    pub fn new_with(mode: EnvMode, user: bool, is_repainting: bool) -> Self {
        Self {
            mode,
            user,
            is_repainting,
        }
    }
    pub fn new_at_early_startup(mode: EnvMode) -> Self {
        Self::new_with(mode, false, false)
    }
}

/// A collection of status and pipestatus.
#[derive(Clone, Debug)]
pub struct Statuses {
    /// Status of the last job to exit.
    pub status: c_int,

    /// Signal from the most recent process in the last job that was terminated by a signal.
    /// None if all processes exited normally.
    pub kill_signal: Option<RawSignal>,

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

#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct EnvVarFlags {
    // Whether the variable is exported.
    pub exported: bool,
    // Whether the variable is read only.
    pub read_only: bool,
    // Whether the variable is a path variable.
    pub pathvar: bool,
}

/// EnvVar is an immutable value-type data structure representing the value of an environment
/// variable.
#[derive(Clone, Debug, PartialEq)]
pub struct EnvVar {
    /// The list of values in this variable.
    /// Arc allows for cheap copying
    values: Arc<[WString]>,
    /// The variable's flags.
    flags: EnvVarFlags,
}

impl Default for EnvVar {
    fn default() -> Self {
        use std::sync::LazyLock;
        /// A shared read-only empty list.
        static EMPTY_LIST: LazyLock<Arc<[WString]>> = LazyLock::new(|| Arc::new([]));

        EnvVar {
            values: Arc::clone(&*EMPTY_LIST),
            flags: EnvVarFlags::default(),
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
        self.flags.exported
    }

    /// Returns whether the variable is a path variable.
    pub fn is_pathvar(&self) -> bool {
        self.flags.pathvar
    }

    /// Returns whether the variable is read-only.
    pub fn is_read_only(&self) -> bool {
        self.flags.read_only
    }

    /// Returns the variable's flags.
    pub fn flags(&self) -> EnvVarFlags {
        self.flags
    }

    /// Returns the variable's value as a string.
    pub fn as_string(&self) -> WString {
        join_strings(&self.values, self.delimiter())
    }

    /// Returns the variable's values.
    pub fn as_list(&self) -> &[WString] {
        &self.values
    }

    /// Returns the delimiter character used when converting from a list to a string.
    pub fn delimiter(&self) -> char {
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
        flags.exported = export;
        EnvVar {
            values: self.values.clone(),
            flags,
        }
    }

    /// Returns a copy of the variable with the path variable flag changed.
    pub fn setting_pathvar(&self, pathvar: bool) -> Self {
        let mut flags = self.flags;
        flags.pathvar = pathvar;
        EnvVar {
            values: self.values.clone(),
            flags,
        }
    }

    /// Returns flags for a variable with the given name.
    pub fn flags_for(name: &wstr) -> EnvVarFlags {
        let mut result = EnvVarFlags::default();
        if is_read_only(name) {
            result.read_only = true;
        }
        result
    }
}

pub type VarTable = HashMap<WString, EnvVar>;

pub const FISH_TERMINAL_COLOR_THEME_VAR: &wstr = L!("fish_terminal_color_theme");

#[cfg(test)]
mod tests {
    use super::{EnvMode, EnvVar, EnvVarFlags};
    use crate::env::EnvSetMode;
    use crate::env::environment::{EnvStack, Environment as _};
    use crate::prelude::*;
    use crate::tests::prelude::*;
    use assert_matches::assert_matches;
    use std::{
        mem::MaybeUninit,
        time::{SystemTime, UNIX_EPOCH},
    };

    /// Helper for test_timezone_env_vars().
    fn return_timezone_hour(tstamp: SystemTime, timezone: &wstr) -> libc::c_int {
        let vars = EnvStack::globals().create_child(true /* dispatches_var_changes */);

        vars.set_one(
            L!("TZ"),
            EnvSetMode::new(EnvMode::EXPORTED, false),
            timezone.to_owned(),
        );

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
        assert_matches!(delta, 1 | -23);
    }

    // Verify that setting special env vars have the expected effect on the current shell process.
    #[test]
    #[serial]
    fn test_env_vars() {
        test_init();
        test_timezone_env_vars();
        // TODO: Add tests for the locale vars.

        let exported = EnvVarFlags {
            exported: true,
            ..Default::default()
        };
        let v1 = EnvVar::new(L!("abc").to_owned(), exported);
        let v2 = EnvVar::new_vec(vec![L!("abc").to_owned()], exported);
        let v3 = EnvVar::new_vec(vec![L!("abc").to_owned()], EnvVarFlags::default());
        let v4 = EnvVar::new_vec(vec![L!("abc").to_owned(), L!("def").to_owned()], exported);
        assert_eq!(v1, v2);
        assert_ne!(v1, v3);
        assert_ne!(v1, v4);
    }
}
