use crate::env::{EnvVar, r#impl::environment::EnvScopedImpl};
use fish_widestring::wstr;

pub fn is_electric_var(name: &wstr) -> bool {
    ElectricVar::for_name(name).is_some()
}

/// Check if a variable may not be set using the set command.
pub fn is_read_only(name: &wstr) -> bool {
    ElectricVar::for_name(name).is_some_and(|var| var.readonly())
}

type Getter = fn(&EnvScopedImpl) -> EnvVar;

pub(super) enum ElectricValue {
    Regular,
    Computed(Getter),
}

pub(super) struct ElectricVar {
    pub(super) name: &'static wstr,
    readonly: bool,
    exported: bool,
    value: ElectricValue,
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
        self.readonly
    }

    pub(super) fn compute(&self, env: &EnvScopedImpl) -> Option<EnvVar> {
        use ElectricValue::*;
        match self.value {
            Regular => None,
            Computed(getter) => Some(getter(env)),
        }
    }

    pub(super) fn exports(&self) -> bool {
        self.exported
    }

    const fn new_default(name: &'static wstr, value: ElectricValue) -> ElectricVar {
        ElectricVar {
            name,
            readonly: true,
            exported: false,
            value,
        }
    }
    const fn new_exported(name: &'static wstr, value: ElectricValue) -> ElectricVar {
        ElectricVar {
            name,
            readonly: true,
            exported: true,
            value,
        }
    }
    const fn new_writable(name: &'static wstr, value: ElectricValue) -> ElectricVar {
        ElectricVar {
            name,
            readonly: false,
            exported: false,
            value,
        }
    }
}

pub(super) mod electric_values {
    use super::{
        ElectricValue::{self, *},
        ElectricVar, Getter,
    };
    use crate::{
        env::{
            EnvMode, EnvVar, EnvVarFlags, FISH_BIN_DIR, FISH_CACHE_DIR, FISH_CONFIG_DIR,
            FISH_DATADIR_VAR, FISH_HELPDIR_VAR, FISH_MANDIR_VAR, FISH_SYSCONFDIR_VAR,
            FISH_TERMINAL_COLOR_THEME_VAR, FISH_USER_DATA_DIR,
        },
        history::{History, history_id_from_var},
        kill::kill_entries,
        reader::{commandline_get_state, reader_status_count},
        threads::is_main_thread,
    };
    use fish_common::assert_sorted_by_name;
    use fish_widestring::{L, ToWString as _, wstr};
    use nix::sys::stat::{Mode, umask};

    const fn var(name: &'static wstr, value: ElectricValue) -> ElectricVar {
        ElectricVar::new_default(name, value)
    }
    const fn exported_var(name: &'static wstr, value: ElectricValue) -> ElectricVar {
        ElectricVar::new_exported(name, value)
    }
    const fn writable_var(name: &'static wstr, value: ElectricValue) -> ElectricVar {
        ElectricVar::new_writable(name, value)
    }

    // Keep sorted alphabetically
    pub(in super::super) const ELECTRIC_VARIABLES: &[ElectricVar] = &[
        var(L!("FISH_VERSION"), Regular),
        exported_var(L!("PWD"), Computed(GET_PWD)),
        exported_var(L!("SHLVL"), Regular),
        var(L!("_"), Regular),
        var(FISH_BIN_DIR, Regular),
        var(FISH_CACHE_DIR, Regular),
        var(FISH_CONFIG_DIR, Regular),
        var(FISH_DATADIR_VAR, Regular),
        var(FISH_HELPDIR_VAR, Regular),
        var(FISH_MANDIR_VAR, Regular),
        var(FISH_SYSCONFDIR_VAR, Regular),
        var(FISH_USER_DATA_DIR, Regular),
        var(L!("fish_kill_signal"), Computed(GET_FISH_KILL_SIGNAL)),
        var(L!("fish_killring"), Computed(GET_FISH_KILLRING)),
        var(L!("fish_pid"), Regular),
        var(FISH_TERMINAL_COLOR_THEME_VAR, Regular),
        var(L!("history"), Computed(GET_HISTORY)),
        var(L!("hostname"), Regular),
        var(L!("pipestatus"), Computed(GET_PIPESTATUS)),
        var(L!("status"), Computed(GET_STATUS)),
        var(L!("status_generation"), Computed(GET_STATUS_GENERATION)),
        writable_var(L!("umask"), Computed(GET_UMASK)),
        var(L!("version"), Regular),
    ];
    assert_sorted_by_name!(ELECTRIC_VARIABLES);

    const GET_PWD: Getter = |env| EnvVar::new(env.perproc_data.pwd.clone(), EnvVarFlags::EXPORT);
    const GET_FISH_KILL_SIGNAL: Getter = |env| {
        let js = &env.perproc_data.statuses;
        let signal = js.kill_signal.map_or(0, |ks| ks.code());
        EnvVar::new_from_name(L!("fish_kill_signal"), signal.to_wstring())
    };
    const GET_FISH_KILLRING: Getter =
        |_env| EnvVar::new_from_name_vec(L!("fish_killring"), kill_entries());
    const GET_HISTORY: Getter = |env| {
        // Big hack. We only allow getting the history on the main thread. Note that history_t
        // may ask for an environment variable, so don't take the lock here (we don't need it).
        if !is_main_thread() {
            return EnvVar::new_from_name_vec(L!("history"), vec![]);
        }
        let history = commandline_get_state(true).history.unwrap_or_else(|| {
            let fish_history_var = env.getf(L!("fish_history"), EnvMode::default());
            let history_id = history_id_from_var(fish_history_var);
            History::new(history_id)
        });
        EnvVar::new_from_name_vec(L!("history"), history.get_history())
    };
    const GET_PIPESTATUS: Getter = |env| {
        let js = &env.perproc_data.statuses;
        let mut result = Vec::with_capacity(js.pipestatus.len());
        for i in &js.pipestatus {
            result.push(i.to_wstring());
        }
        EnvVar::new_from_name_vec(L!("pipestatus"), result)
    };
    const GET_STATUS: Getter = |env| {
        let js = &env.perproc_data.statuses;
        EnvVar::new_from_name(L!("status"), js.status.to_wstring())
    };
    const GET_STATUS_GENERATION: Getter =
        |_env| EnvVar::new_from_name(L!("status_generation"), reader_status_count().to_wstring());
    const GET_UMASK: Getter = |_env| {
        // note umask() is an absurd API: you call it to set the value and it returns the old
        // value. Thus we have to call it twice, to reset the value. The env_lock protects
        // against races. Guess what the umask is; if we guess right we don't need to reset it.
        let guess = Mode::S_IWGRP | Mode::S_IWOTH;
        let res = umask(guess);
        if res != guess {
            umask(res);
        }
        EnvVar::new_from_name(L!("umask"), sprintf!("0%0.3o", res.bits()))
    };
}
pub(super) use electric_values::ELECTRIC_VARIABLES;
