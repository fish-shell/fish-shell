// A mdoule concerned with the exec side of fork/exec.
// This concerns posix_spawn support, and async-signal
// safe code which happens in between fork and exec.

mod flog_safe;
pub mod postfork;
pub mod spawn;
use crate::ffi::job_t;

/// Get the list of signals which should be blocked for a given job.
/// Return true if at least one signal was set.
fn blocked_signals_for_job(job: &job_t, sigmask: &mut libc::sigset_t) -> bool {
    // Block some signals in background jobs for which job control is turned off (#6828).
    if !job.is_foreground() && !job.wants_job_control() {
        unsafe {
            libc::sigaddset(sigmask, libc::SIGINT);
            libc::sigaddset(sigmask, libc::SIGQUIT);
        }
        return true;
    }
    false
}
