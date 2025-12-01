// A module concerned with the exec side of fork/exec.
// This concerns posix_spawn support, and async-signal
// safe code which happens in between fork and exec.

pub mod flog_safe;
pub mod postfork;
#[cfg(have_posix_spawn)]
pub mod spawn;
use crate::proc::Job;
use libc::{SIGINT, SIGQUIT};

/// Get the list of signals which should be blocked for a given job.
/// Return true if at least one signal was set.
pub fn blocked_signals_for_job(job: &Job, sigmask: &mut libc::sigset_t) -> bool {
    // Block some signals in background jobs for which job control is turned off (#6828).
    if !job.is_foreground() && !job.wants_job_control() {
        unsafe {
            libc::sigaddset(sigmask, SIGINT);
            libc::sigaddset(sigmask, SIGQUIT);
        }
        return true;
    }
    false
}

// Bravely define _PATH_BSHELL. On practice it's /bin/sh everywhere, except on Android.
// NOTE: this is currently duplicated in __fish_posix_shell.
#[cfg(not(target_os = "android"))]
pub static PATH_BSHELL: &[u8] = b"/bin/sh\0";

#[cfg(target_os = "android")]
pub static PATH_BSHELL: &[u8] = b"/system/bin/sh\0";
