use crate::env::{EnvStack, Environment};
use crate::wchar::wstr;
use crate::wchar::L;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

// Limit `read` to 100 MiB (bytes not wide chars) by default. This can be overridden by the
// fish_read_limit variable.
const DEFAULT_READ_BYTE_LIMIT: usize = 100 * 1024 * 1024;
pub static READ_BYTE_LIMIT: AtomicUsize = AtomicUsize::new(DEFAULT_READ_BYTE_LIMIT);

/// Whether to use `posix_spawn()` when possible.
static USE_POSIX_SPAWN: AtomicBool = AtomicBool::new(false);

pub fn env_dispatch_init(vars: &dyn Environment) {
    todo!()
}

/// React to modifying the given variable.
pub fn env_dispatch_var_change(key: &wstr, vars: &mut EnvStack) {
    todo!()
}

pub fn use_posix_spawn() -> bool {
    USE_POSIX_SPAWN.load(Ordering::Relaxed)
}

/// Whether or not we are running on an OS where we allow ourselves to use `posix_spawn()`.
const fn allow_use_posix_spawn() -> bool {
    // OpenBSD's posix_spawn returns status 127, instead of erroring with ENOEXEC, when faced with a
    // shebangless script. Disable posix_spawn on OpenBSD.
    if cfg!(target_os = "openbsd") {
        false
    } else if cfg!(not(target_os = "linux")) {
        true
    } else {
        // The C++ code used __GLIBC_PREREQ(2, 24) && !defined(__UCLIBC__) to determine if we'll use
        // posix_spawn() by default on Linux. Surprise! We don't have to worry about porting that
        // logic here because the libc crate only supports 2.26+ atm.
        // See https://github.com/rust-lang/libc/issues/1412
        true
    }
}

fn handle_fish_use_posix_spawn_change(vars: &dyn Environment) {
    // Note that if the variable is missing or empty we default to true (if allowed).
    if !allow_use_posix_spawn() {
        USE_POSIX_SPAWN.store(false, Ordering::Relaxed);
    } else if let Some(var) = vars.get(L!("fish_use_posix_spawn")) {
        let use_posix_spawn =
            var.is_empty() || crate::wcstringutil::bool_from_string(&var.as_string());
        USE_POSIX_SPAWN.store(use_posix_spawn, Ordering::Relaxed);
    } else {
        USE_POSIX_SPAWN.store(true, Ordering::Relaxed);
    }
}
