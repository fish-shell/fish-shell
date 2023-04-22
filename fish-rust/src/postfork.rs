use crate::null_terminated_array::AsNullTerminatedArray;
use crate::proc::{Job, Process};
use crate::redirection::Dup2List;
use errno::Errno;
use libc::{c_char, c_int, pid_t};
use std::ffi::CStr;

pub fn safe_report_exec_error(
    err: Errno,
    actual_cmd: &CStr,
    argv: &impl AsNullTerminatedArray<CharType = c_char>,
    envv: &impl AsNullTerminatedArray<CharType = c_char>,
) {
    todo!()
}

pub fn child_setup_process(
    claim_tty_from: pid_t,
    job: &Job,
    is_forked: bool,
    dup2s: &Dup2List,
) -> c_int {
    todo!()
}

pub fn execute_setpgid(pid: pid_t, pgroup: pid_t, is_parent: bool) -> c_int {
    todo!()
}

pub fn report_setpgid_error(
    err: c_int,
    is_parent: bool,
    desired_pgid: pid_t,
    j: &Job,
    p: &Process,
) {
    todo!()
}

pub fn execute_fork() -> pid_t {
    todo!()
}

#[cfg(FISH_USE_POSIX_SPAWN)]
/// A RAII type which wraps up posix_spawn's data structures.
pub struct PosixSpawner {
    error: c_int,
    attr: Option<libc::posix_spawnattr_t>,
    actions: Option<libc::posix_spawn_file_actions_t>,
}

#[cfg(FISH_USE_POSIX_SPAWN)]
impl PosixSpawner {
    /// Attempt to construct from a job and dup2 list.
    /// The caller must check the error function, as this may fail.
    pub fn new(j: &Job, dup2s: &Dup2List) -> Self {
        todo!()
    }

    /// \return the last error code, or 0 if there is no error.
    pub fn get_error(&self) -> c_int {
        self.error
    }

    /// If this spawner does not have an error, invoke posix_spawn. Parameters are the same as
    /// posix_spawn.
    /// \return the pid, or none() on failure, in which case our error will be set.
    pub fn spawn(
        cmd: &CStr,
        argv: impl AsNullTerminatedArray<CharType = c_char>,
        envp: impl AsNullTerminatedArray<CharType = c_char>,
    ) -> Option<pid_t> {
        todo!()
    }

    fn check_fail(&mut self, err: c_int) -> bool {
        todo!();
    }
    fn attr(&self) -> &libc::posix_spawn_attr_t {
        self.attr.as_ref().unwrap()
    }
    fn actions(&self) -> &libc::posix_spawn_file_actions_t {
        self.actions.as_ref().unwrap()
    }
}
