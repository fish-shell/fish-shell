use crate::env::FISH_TERMINAL_COLOR_THEME_VAR;
use fish_common::assert_sorted_by_name;
use fish_widestring::{L, wstr};

pub fn is_electric_var(name: &wstr) -> bool {
    ElectricVar::for_name(name).is_some()
}

/// Check if a variable may not be set using the set command.
pub fn is_read_only(name: &wstr) -> bool {
    ElectricVar::for_name(name).is_some_and(|var| var.readonly())
}

mod electric {
    pub(super) const READONLY: u8 = 1 << 0; // May not be modified by the user.
    pub(super) const COMPUTED: u8 = 1 << 1; // Value is dynamically computed.
    pub(super) const EXPORTS: u8 = 1 << 2; // Exported to child processes.
    pub(super) type ElectricVarFlags = u8;
}

pub(super) struct ElectricVar {
    pub(super) name: &'static wstr,
    flags: electric::ElectricVarFlags,
}

impl ElectricVar {
    const fn new(name: &'static wstr, flags: electric::ElectricVarFlags) -> Self {
        Self { name, flags }
    }
}

impl ElectricVar {
    /// Return the ElectricVar with the given name, if any
    pub(super) fn for_name(name: &wstr) -> Option<&'static ElectricVar> {
        match ELECTRIC_VARIABLES.binary_search_by(|ev| ev.name.cmp(name)) {
            Ok(idx) => Some(&ELECTRIC_VARIABLES[idx]),
            Err(_) => None,
        }
    }

    pub(super) fn readonly(&self) -> bool {
        self.flags & electric::READONLY != 0
    }

    pub(super) fn computed(&self) -> bool {
        self.flags & electric::COMPUTED != 0
    }

    pub(super) fn exports(&self) -> bool {
        self.flags & electric::EXPORTS != 0
    }
}

// Keep sorted alphabetically
pub(super) const ELECTRIC_VARIABLES: &[ElectricVar] = {
    use electric::{COMPUTED, EXPORTS, READONLY};
    let v = ElectricVar::new;
    &[
        v(L!("FISH_VERSION"), READONLY),
        v(L!("PWD"), READONLY | COMPUTED | EXPORTS),
        v(L!("SHLVL"), READONLY | EXPORTS),
        v(L!("_"), READONLY),
        v(L!("fish_kill_signal"), READONLY | COMPUTED),
        v(L!("fish_killring"), READONLY | COMPUTED),
        v(L!("fish_pid"), READONLY),
        v(FISH_TERMINAL_COLOR_THEME_VAR, READONLY),
        v(L!("history"), READONLY | COMPUTED),
        v(L!("hostname"), READONLY),
        v(L!("pipestatus"), READONLY | COMPUTED),
        v(L!("status"), READONLY | COMPUTED),
        v(L!("status_generation"), READONLY | COMPUTED),
        v(L!("umask"), COMPUTED),
        v(L!("version"), READONLY),
    ]
};
assert_sorted_by_name!(ELECTRIC_VARIABLES);
