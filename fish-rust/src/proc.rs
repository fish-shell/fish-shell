//! Utilities for keeping track of jobs, processes and subshells, as well as signal handling
//! functions for tracking children. These functions do not themselves launch new processes,
//! the exec library will call proc to create representations of the running jobs as needed.

use crate::ast;
use crate::common::{escape, fputws, redirect_tty_output, scoped_push, timef, Timepoint};
use crate::compat::cur_term;
use crate::env::Statuses;
use crate::event::{self, Event};
use crate::flog::{FLOG, FLOGF};
use crate::global_safety::RelaxedAtomicBool;
use crate::io::IoChain;
use crate::job_group::{JobGroup, MaybeJobId};
use crate::parse_tree::ParsedSourceRef;
use crate::parser::{Block, Parser};
use crate::reader::{fish_is_unwinding_for_exit, reader_schedule_prompt_repaint};
use crate::redirection::RedirectionSpecList;
use crate::signal::{signal_set_handlers_once, Signal};
use crate::topic_monitor::{
    generation_list_t, invalid_generations, topic_monitor_principal, topic_t,
};
use crate::wait_handle::{InternalJobId, WaitHandle, WaitHandleRef, WaitHandleStore};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::ToWString;
use crate::wutil::{perror, wbasename, wgettext, wperror};
use libc::{
    EBADF, EINVAL, ENOTTY, EPERM, EXIT_SUCCESS, SIGABRT, SIGBUS, SIGCONT, SIGFPE, SIGHUP, SIGILL,
    SIGINT, SIGPIPE, SIGQUIT, SIGSEGV, SIGSYS, SIGTTOU, SIG_DFL, SIG_IGN, STDIN_FILENO,
    STDOUT_FILENO, WCONTINUED, WEXITSTATUS, WIFCONTINUED, WIFEXITED, WIFSIGNALED, WIFSTOPPED,
    WNOHANG, WTERMSIG, WUNTRACED, _SC_CLK_TCK,
};
use once_cell::sync::Lazy;
use printf_compat::sprintf;
use std::ffi::CString;
use std::fs;
use std::io::{Read, Write};
use std::os::fd::RawFd;
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicU8, Ordering};
use std::sync::{Arc, Mutex, RwLock, RwLockReadGuard, RwLockWriteGuard};
use widestring_suffix::widestrs;

/// Types of processes.
#[derive(Default, Eq, PartialEq)]
pub enum ProcessType {
    /// A regular external command.
    #[default]
    external,
    /// A builtin command.
    builtin,
    /// A shellscript function.
    function,
    /// A block of commands, represented as a node.
    block_node,
    /// The exec builtin.
    exec,
}

#[repr(u8)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum JobControl {
    all,
    interactive,
    none,
}

/// A number of clock ticks.
pub type ClockTicks = u64;

/// \return clock ticks in seconds, or 0 on failure.
/// This uses sysconf(_SC_CLK_TCK) to convert to seconds.
pub fn clock_ticks_to_seconds(ticks: ClockTicks) -> f64 {
    let clock_ticks_per_sec = unsafe { libc::sysconf(_SC_CLK_TCK) };
    if clock_ticks_per_sec > 0 {
        return ticks as f64 / clock_ticks_per_sec as f64;
    }
    0.0
}

pub type JobGroupRef = Arc<RwLock<JobGroup>>;

/// A proc_status_t is a value type that encapsulates logic around exited vs stopped vs signaled,
/// etc.
#[derive(Clone, Copy, Default)]
pub struct ProcStatus {
    status: i32,

    /// If set, there is no actual status to report, e.g. background or variable assignment.
    empty: bool,
}

impl ProcStatus {
    fn new(status: i32, empty: bool) -> Self {
        ProcStatus { status, empty }
    }

    /// Encode a return value \p ret and signal \p sig into a status value like waitpid() does.
    const fn w_exitcode(ret: i32, sig: i32) -> i32 {
        #[cfg(HAVE_WAITSTATUS_SIGNAL_RET)]
        // It's encoded signal and then status
        // The return status is in the lower byte.
        return (sig << 8) | ret;
        #[cfg(not(HAVE_WAITSTATUS_SIGNAL_RET))]
        // The status is encoded in the upper byte.
        // This should be W_EXITCODE(ret, sig) but that's not available everywhere.
        return (ret << 8) | sig;
    }

    /// Construct from a status returned from a waitpid call.
    pub fn from_waitpid(status: i32) -> ProcStatus {
        ProcStatus::new(status, false)
    }

    /// Construct directly from an exit code.
    pub fn from_exit_code(ret: i32) -> ProcStatus {
        assert!(
            ret >= 0,
            "trying to create proc_status_t from failed waitid()/waitpid() call \
               or invalid builtin exit code!"
        );

        // Some paranoia.
        const zerocode: i32 = ProcStatus::w_exitcode(0, 0);
        const _: () = assert!(
            WIFEXITED(zerocode),
            "Synthetic exit status not reported as exited"
        );

        assert!(ret < 256);
        ProcStatus::new(Self::w_exitcode(ret, 0 /* sig */), false)
    }

    /// Construct directly from a signal.
    pub fn from_signal(signal: Signal) -> ProcStatus {
        ProcStatus::new(Self::w_exitcode(0 /* ret */, signal.code()), false)
    }

    /// Construct an empty status_t (e.g. `set foo bar`).
    pub fn empty() -> ProcStatus {
        let empty = true;
        ProcStatus::new(0, empty)
    }

    /// \return if we are stopped (as in SIGSTOP).
    pub fn stopped(&self) -> bool {
        WIFSTOPPED(self.status)
    }

    /// \return if we are continued (as in SIGCONT).
    pub fn continued(&self) -> bool {
        WIFCONTINUED(self.status)
    }

    /// \return if we exited normally (not a signal).
    pub fn normal_exited(&self) -> bool {
        WIFEXITED(self.status)
    }

    /// \return if we exited because of a signal.
    pub fn signal_exited(&self) -> bool {
        WIFSIGNALED(self.status)
    }

    /// \return the signal code, given that we signal exited.
    pub fn signal_code(&self) -> libc::c_int {
        assert!(self.signal_exited(), "Process is not signal exited");
        WTERMSIG(self.status)
    }

    /// \return the exit code, given that we normal exited.
    pub fn exit_code(&self) -> libc::c_int {
        assert!(self.normal_exited(), "Process is not normal exited");
        WEXITSTATUS(self.status)
    }

    /// \return if this status represents success.
    pub fn is_success(&self) -> bool {
        self.normal_exited() && self.exit_code() == EXIT_SUCCESS
    }

    /// \return if this status is empty.
    pub fn is_empty(&self) -> bool {
        self.empty
    }

    /// \return the value appropriate to populate $status.
    pub fn status_value(&self) -> i32 {
        if self.signal_exited() {
            128 + self.signal_code()
        } else if self.normal_exited() {
            self.exit_code()
        } else {
            panic!("Process is not exited")
        }
    }
}

#[derive(Default)]
struct AtomicProcStatus(AtomicU64);
impl AtomicProcStatus {
    fn load(&self, ordering: Ordering) -> ProcStatus {
        let raw = self.0.load(ordering);
        ProcStatus {
            status: (raw & 0xFFFFFFF) as i32,
            empty: ((raw >> 32) & 0xFF) != 0,
        }
    }
    fn store(&self, s: ProcStatus, ordering: Ordering) {
        let raw = s.status as u64 | ((s.empty as u64) << 32);
        self.0.store(raw, ordering);
    }
}

/// A structure representing a "process" internal to fish. This is backed by a pthread instead of a
/// separate process.
pub struct InternalProc {
    /// An identifier for internal processes.
    /// This is used for logging purposes only.
    internal_proc_id: u64,

    /// Whether the process has exited.
    exited: AtomicBool,

    /// If the process has exited, its status code.
    status: AtomicProcStatus,
}

impl InternalProc {
    pub fn new() -> Self {
        static NEXT_PROC_ID: AtomicU64 = AtomicU64::new(0);
        Self {
            internal_proc_id: NEXT_PROC_ID.fetch_add(1, Ordering::SeqCst),
            exited: AtomicBool::new(false),
            status: AtomicProcStatus::default(),
        }
    }

    /// \return if this process has exited.
    pub fn exited(&self) -> bool {
        self.exited.load(Ordering::Acquire)
    }

    /// Mark this process as exited, with the given status.
    pub fn mark_exited(&mut self, status: ProcStatus) {
        assert!(!self.exited(), "Process is already exited");
        self.status.store(status, Ordering::Relaxed);
        self.exited.store(true, Ordering::Release);
        topic_monitor_principal().post(topic_t::internal_exit);
        FLOG!(
            proc_internal_proc,
            "Internal proc",
            self.internal_proc_id,
            "exited with status",
            status.status_value()
        );
    }

    pub fn get_status(&self) -> ProcStatus {
        assert!(self.exited(), "Process is not exited");
        self.status.load(Ordering::Relaxed)
    }

    pub fn get_id(&self) -> u64 {
        self.internal_proc_id
    }
}

/// 0 should not be used; although it is not a valid PGID in userspace,
///   the Linux kernel will use it for kernel processes.
/// -1 should not be used; it is a possible return value of the getpgid()
///   function
pub const INVALID_PID: i32 = -2;

// Allows transferring the tty to a job group, while it runs.
#[derive(Default)]
pub struct TtyTransfer {
    // The job group which owns the tty, or empty if none.
    owner: Option<JobGroupRef>,
}

impl TtyTransfer {
    pub fn new() -> Self {
        Default::default()
    }
    /// Transfer to the given job group, if it wants to own the terminal.
    #[allow(clippy::wrong_self_convention)]
    pub fn to_job_group(&mut self, jg: &JobGroupRef) {
        assert!(self.owner.is_some(), "Terminal already transferred");
        if TtyTransfer::try_transfer(&jg.read().unwrap()) {
            self.owner = Some(jg.clone());
        }
    }

    /// Reclaim the tty if we transferred it.
    pub fn reclaim(&mut self) {
        if let Some(owner) = self.owner.as_ref() {
            FLOG!(proc_pgroup, "fish reclaiming terminal");
            if unsafe { libc::tcsetpgrp(STDIN_FILENO, libc::getpgrp()) } == -1 {
                FLOGF!(warning, "Could not return shell to foreground");
                perror("tcsetpgrp");
            }
        }
        self.owner = None;
    }

    /// Save the current tty modes into the owning job group, if we are transferred.
    pub fn save_tty_modes(&mut self) {
        if let Some(ref mut owner) = self.owner {
            let mut tmodes: libc::termios = unsafe { std::mem::zeroed() };
            if unsafe { libc::tcgetattr(STDIN_FILENO, &mut tmodes) } == 0 {
                owner.write().unwrap().tmodes = Some(tmodes);
            } else if errno::errno().0 != ENOTTY {
                perror("tcgetattr");
            }
        }
    }

    fn try_transfer(jg: &JobGroup) -> bool {
        if !jg.wants_terminal() {
            // The job doesn't want the terminal.
            return false;
        }

        // Get the pgid; we must have one if we want the terminal.
        let pgid = jg.get_pgid().unwrap();
        assert!(pgid >= 0, "Invalid pgid");

        // It should never be fish's pgroup.
        let fish_pgrp = unsafe { libc::getpgrp() };
        assert!(pgid != fish_pgrp, "Job should not have fish's pgroup");

        // Ok, we want to transfer to the child.
        // Note it is important to be very careful about calling tcsetpgrp()!
        // fish ignores SIGTTOU which means that it has the power to reassign the tty even if it doesn't
        // own it. This means that other processes may get SIGTTOU and become zombies.
        // Check who own the tty now. There's four cases of interest:
        //   1. There is no tty at all (tcgetpgrp() returns -1). For example running from a pure script.
        //      Of course do not transfer it in that case.
        //   2. The tty is owned by the process. This comes about often, as the process will call
        //      tcsetpgrp() on itself between fork ane exec. This is the essential race inherent in
        //      tcsetpgrp(). In this case we want to reclaim the tty, but do not need to transfer it
        //      ourselves since the child won the race.
        //   3. The tty is owned by a different process. This may come about if fish is running in the
        //      background with job control enabled. Do not transfer it.
        //   4. The tty is owned by fish. In that case we want to transfer the pgid.
        let current_owner = unsafe { libc::tcgetpgrp(STDIN_FILENO) };
        if current_owner < 0 {
            // Case 1.
            return false;
        } else if current_owner == pgid {
            // Case 2.
            return true;
        } else if current_owner != pgid && current_owner != fish_pgrp {
            // Case 3.
            return false;
        }
        // Case 4 - we do want to transfer it.

        // The tcsetpgrp(2) man page says that EPERM is thrown if "pgrp has a supported value, but
        // is not the process group ID of a process in the same session as the calling process."
        // Since we _guarantee_ that this isn't the case (the child calls setpgid before it calls
        // SIGSTOP, and the child was created in the same session as us), it seems that EPERM is
        // being thrown because of an caching issue - the call to tcsetpgrp isn't seeing the
        // newly-created process group just yet. On this developer's test machine (WSL running Linux
        // 4.4.0), EPERM does indeed disappear on retry. The important thing is that we can
        // guarantee the process isn't going to exit while we wait (which would cause us to possibly
        // block indefinitely).
        while unsafe { libc::tcsetpgrp(STDIN_FILENO, pgid) } != 0 {
            FLOGF!(proc_termowner, "tcsetpgrp failed: %d", errno::errno());

            // Before anything else, make sure that it's even necessary to call tcsetpgrp.
            // Since it usually _is_ necessary, we only check in case it fails so as to avoid the
            // unnecessary syscall and associated context switch, which profiling has shown to have
            // a significant cost when running process groups in quick succession.
            let getpgrp_res = unsafe { libc::tcgetpgrp(STDIN_FILENO) };
            if getpgrp_res < 0 {
                match errno::errno().0 {
                    ENOTTY => {
                        // stdin is not a tty. This may come about if job control is enabled but we are
                        // not a tty - see #6573.
                        return false;
                    }
                    EBADF => {
                        // stdin has been closed. Workaround a glibc bug - see #3644.
                        redirect_tty_output();
                        return false;
                    }
                    _ => {
                        perror("tcgetpgrp");
                        return false;
                    }
                }
            }
            if getpgrp_res == pgid {
                FLOGF!(
                    proc_termowner,
                    "Process group %d already has control of terminal",
                    pgid
                );
                return true;
            }

            let mut pgroup_terminated;
            if errno::errno().0 == EINVAL {
                // OS X returns EINVAL if the process group no longer lives. Probably other OSes,
                // too. Unlike EPERM below, EINVAL can only happen if the process group has
                // terminated.
                pgroup_terminated = true;
            } else if errno::errno().0 == EPERM {
                // Retry so long as this isn't because the process group is dead.
                let mut result: libc::c_int = 0;
                let wait_result = unsafe { libc::waitpid(-pgid, &mut result, WNOHANG) };
                if wait_result == -1 {
                    // Note that -1 is technically an "error" for waitpid in the sense that an
                    // invalid argument was specified because no such process group exists any
                    // longer. This is the observed behavior on Linux 4.4.0. a "success" result
                    // would mean processes from the group still exist but is still running in some
                    // state or the other.
                    pgroup_terminated = true;
                } else {
                    // Debug the original tcsetpgrp error (not the waitpid errno) to the log, and
                    // then retry until not EPERM or the process group has exited.
                    FLOGF!(
                        proc_termowner,
                        "terminal_give_to_job(): EPERM with pgid %d.",
                        pgid
                    );
                    continue;
                }
            } else if errno::errno().0 == ENOTTY {
                // stdin is not a TTY. In general we expect this to be caught via the tcgetpgrp
                // call's EBADF handler above.
                return false;
            } else {
                FLOGF!(
                    warning,
                    "Could not send job %d ('%ls') with pgid %d to foreground",
                    jg.job_id.to_wstring(),
                    jg.command,
                    pgid
                );
                perror("tcsetpgrp");
                return false;
            }

            if pgroup_terminated {
                // All processes in the process group has exited.
                // Since we delay reaping any processes in a process group until all members of that
                // job/group have been started, the only way this can happen is if the very last
                // process in the group terminated and didn't need to access the terminal, otherwise
                // it would have hung waiting for terminal IO (SIGTTIN). We can safely ignore this.
                FLOGF!(
                    proc_termowner,
                    "tcsetpgrp called but process group %d has terminated.\n",
                    pgid
                );
                return false;
            }

            break;
        }
        true
    }
}

/// The destructor will assert if reclaim() has not been called.
impl Drop for TtyTransfer {
    fn drop(&mut self) {
        assert!(self.owner.is_none(), "Forgot to reclaim() the tty");
    }
}

/// A structure representing a single fish process. Contains variables for tracking process state
/// and the process argument list. Actually, a fish process can be either a regular external
/// process, an internal builtin which may or may not spawn a fake IO process during execution, a
/// shellscript function or a block of commands to be evaluated by calling eval. Lastly, this
/// process can be the result of an exec command. The role of this process_t is determined by the
/// type field, which can be one of process_type_t::external, process_type_t::builtin,
/// process_type_t::function, process_type_t::exec.
///
/// The process_t contains information on how the process should be started, such as command name
/// and arguments, as well as runtime information on the status of the actual physical process which
/// represents it. Shellscript functions, builtins and blocks of code may all need to spawn an
/// external process that handles the piping and redirecting of IO for them.
///
/// If the process is of type process_type_t::external or process_type_t::exec, argv is the argument
/// array and actual_cmd is the absolute path of the command to execute.
///
/// If the process is of type process_type_t::builtin, argv is the argument vector, and argv[0] is
/// the name of the builtin command.
///
/// If the process is of type process_type_t::function, argv is the argument vector, and argv[0] is
/// the name of the shellscript function.
#[derive(Default)]
pub struct Process {
    /// Note whether we are the first and/or last in the job
    pub is_first_in_job: bool,
    pub is_last_in_job: bool,

    /// Type of process.
    pub typ: ProcessType,

    /// For internal block processes only, the node of the statement.
    /// This is always either block, ifs, or switchs, never boolean or decorated.
    pub block_node_source: Option<ParsedSourceRef>,
    pub internal_block_node: Option<std::ptr::NonNull<ast::Statement>>,

    /// The expanded variable assignments for this process, as specified by the `a=b cmd` syntax.
    pub variable_assignments: Vec<ConcreteAssignment>,

    /// Actual command to pass to exec in case of process_type_t::external or process_type_t::exec.
    pub actual_cmd: WString,

    /// Generation counts for reaping.
    pub gens: generation_list_t,

    /// Process ID
    pub pid: libc::pid_t,

    /// If we are an "internal process," that process.
    // todo! remove RwLock
    pub internal_proc: Option<Arc<RwLock<InternalProc>>>,

    /// File descriptor that pipe output should bind to.
    pub pipe_write_fd: RawFd,

    /// True if process has completed.
    pub completed: bool,

    /// True if process has stopped.
    pub stopped: bool,

    /// If set, this process is (or will become) the pgroup leader.
    /// This is only meaningful for external processes.
    pub leads_pgrp: bool,

    /// Whether we have generated a proc_exit event.
    pub posted_proc_exit: bool,

    /// Reported status value.
    pub status: ProcStatus,

    /// Last time of cpu time check, in seconds (per timef).
    pub last_time: Timepoint,

    /// Number of jiffies spent in process at last cpu time check.
    pub last_jiffies: ClockTicks,

    argv: Vec<WString>,
    proc_redirection_specs: RedirectionSpecList,

    // The wait handle. This is constructed lazily, and cached.
    // This may be null.
    wait_handle: Option<WaitHandleRef>,
}

pub struct ConcreteAssignment {
    pub variable_name: WString,
    pub values: Vec<WString>,
}

impl ConcreteAssignment {
    pub fn new(variable_name: WString, values: Vec<WString>) -> Self {
        Self {
            variable_name,
            values,
        }
    }
}

impl Process {
    pub fn new() -> Self {
        Default::default()
    }

    /// Sets argv.
    pub fn set_argv(&mut self, argv: Vec<WString>) {
        self.argv = argv;
    }

    /// Returns argv.
    pub fn argv(&self) -> &Vec<WString> {
        &self.argv
    }

    /// Returns argv[0].
    pub fn argv0(&self) -> Option<&wstr> {
        self.argv.get(0).map(|s| s.as_utfstr())
    }

    /// Redirection list getter and setter.
    pub fn redirection_specs(&self) -> &RedirectionSpecList {
        &self.proc_redirection_specs
    }
    pub fn redirection_specs_mut(&mut self) -> &mut RedirectionSpecList {
        &mut self.proc_redirection_specs
    }
    pub fn set_redirection_specs(&mut self, specs: RedirectionSpecList) {
        self.proc_redirection_specs = specs;
    }

    /// Store the current topic generations. That is, right before the process is launched, record
    /// the generations of all topics; then we can tell which generation values have changed after
    /// launch. This helps us avoid spurious waitpid calls.
    pub fn check_generations_before_launch(&mut self) {
        self.gens = topic_monitor_principal().current_generations();
    }

    /// Mark that this process was part of a pipeline which was aborted.
    /// The process was never successfully launched; give it a status of EXIT_FAILURE.
    pub fn mark_aborted_before_launch(&mut self) {
        self.completed = true;
        // The status may have already been set to e.g. STATUS_NOT_EXECUTABLE.
        // Only stomp a successful status.
        if self.status.is_success() {
            self.status = ProcStatus::from_exit_code(libc::EXIT_FAILURE);
        }
    }

    /// \return whether this process type is internal (block, function, or builtin).
    pub fn is_internal(&self) -> bool {
        match self.typ {
            ProcessType::builtin | ProcessType::function | ProcessType::block_node => true,
            ProcessType::external | ProcessType::exec => false,
        }
    }

    /// \return the wait handle for the process, if it exists.
    pub fn get_wait_handle(&self) -> Option<WaitHandleRef> {
        self.wait_handle.clone()
    }

    /// Create a wait handle for the process.
    /// As a process does not know its job id, we pass it in.
    /// Note this will return null if the process is not waitable (has no pid).
    pub fn make_wait_handle(&mut self, jid: InternalJobId) -> Option<WaitHandleRef> {
        if self.typ != ProcessType::external || self.pid <= 0 {
            // Not waitable.
            None
        } else {
            if self.wait_handle.is_none() {
                self.wait_handle = Some(WaitHandle::new(
                    self.pid,
                    jid,
                    wbasename(self.actual_cmd.clone()),
                ));
            }
            self.get_wait_handle()
        }
    }
}

pub type ProcessPtr = Box<Process>;
pub type ProcessList = Vec<ProcessPtr>;

/// A set of jobs properties. These are immutable: they do not change for the lifetime of the
/// job.
#[derive(Default, Clone, Copy)]
pub struct JobProperties {
    /// Whether the specified job is a part of a subshell, event handler or some other form of
    /// special job that should not be reported.
    pub skip_notification: bool,

    /// Whether the job had the background ampersand when constructed, e.g. /bin/echo foo &
    /// Note that a job may move between foreground and background; this just describes what the
    /// initial state should be.
    pub initial_background: bool,

    /// Whether the job has the 'time' prefix and so we should print timing for this job.
    pub wants_timing: bool,

    /// Whether this job was created as part of an event handler.
    pub from_event_handler: bool,
}

/// Flags associated with the job.
#[derive(Default)]
pub struct JobFlags {
    /// Whether the specified job is completely constructed: every process in the job has been
    /// forked, etc.
    pub constructed: bool,

    /// Whether the user has been notified that this job is stopped (if it is).
    pub notified_of_stop: bool,

    /// Whether the exit status should be negated. This flag can only be set by the not builtin.
    /// Two "not" prefixes on a single job cancel each other out.
    pub negate: bool,

    /// This job is disowned, and should be removed from the active jobs list.
    pub disown_requested: bool,

    // Indicates that we are the "group root." Any other jobs using this tree are nested.
    pub is_group_root: bool,
}

/// A struct representing a job. A job is a pipeline of one or more processes.
#[derive(Default)]
pub struct Job {
    /// Set of immutable job properties.
    properties: JobProperties,

    /// The original command which led to the creation of this job. It is used for displaying
    /// messages about job status on the terminal.
    command_str: WString,

    /// All the processes in this job.
    pub processes: ProcessList,

    // The group containing this job.
    // This is never cleared.
    pub group: Option<JobGroupRef>,

    /// A non-user-visible, never-recycled job ID.
    pub internal_job_id: InternalJobId,

    /// Flags associated with the job.
    pub job_flags: JobFlags,
}

impl Job {
    pub fn new(properties: JobProperties, command_str: WString) -> Self {
        static NEXT_INTERNAL_JOB_ID: AtomicU64 = AtomicU64::new(0);
        Job {
            properties,
            command_str,
            internal_job_id: NEXT_INTERNAL_JOB_ID.fetch_add(1, Ordering::Relaxed),
            ..Default::default()
        }
    }

    pub fn group(&self) -> RwLockReadGuard<'_, JobGroup> {
        self.group.as_ref().unwrap().read().unwrap()
    }

    pub fn group_mut(&self) -> RwLockWriteGuard<'_, JobGroup> {
        self.group.as_ref().unwrap().write().unwrap()
    }

    /// Returns the command.
    pub fn command(&self) -> &wstr {
        &self.command_str
    }

    /// \return whether it is OK to reap a given process. Sometimes we want to defer reaping a
    /// process if it is the group leader and the job is not yet constructed, because then we might
    /// also reap the process group and then we cannot add new processes to the group.
    pub fn can_reap(&self, p: &ProcessPtr) -> bool {
        !(
            // Can't reap twice.
            p.completed ||
            // Can't reap the group leader in an under-construction job.
            (p.pid != 0 && !self.is_constructed() && self.get_pgid() == Some(p.pid))
        )
    }

    /// Returns a truncated version of the job string. Used when a message has already been emitted
    /// containing the full job string and job id, but using the job id alone would be confusing
    /// due to reuse of freed job ids. Prevents overloading the debug comments with the full,
    /// untruncated job string when we don't care what the job is, only which of the currently
    /// running jobs it is.
    #[widestrs]
    pub fn preview(&self) -> WString {
        if self.processes.is_empty() {
            return ""L.to_owned();
        }
        // Note argv0 may be empty in e.g. a block process.
        let result = self.processes.first().unwrap().argv0().unwrap_or("null"L);
        result.to_owned() + "..."L
    }

    /// \return our pgid, or none if we don't have one, or are internal to fish
    /// This never returns fish's own pgroup.
    pub fn get_pgid(&self) -> Option<libc::pid_t> {
        self.group().get_pgid()
    }

    /// \return the pid of the last external process in the job.
    /// This may be none if the job consists of just internal fish functions or builtins.
    /// This will never be fish's own pid.
    pub fn get_last_pid(&self) -> Option<libc::pid_t> {
        self.processes
            .iter()
            .rev()
            .map(|proc| proc.pid)
            .find(|pid| *pid > 0)
    }

    /// The id of this job.
    /// This is user-visible, is recycled, and may be -1.
    pub fn job_id(&self) -> MaybeJobId {
        self.group().job_id
    }

    /// Access the job flags.
    pub fn flags(&self) -> &JobFlags {
        &self.job_flags
    }

    /// Access mutable job flags.
    pub fn mut_flags(&mut self) -> &mut JobFlags {
        &mut self.job_flags
    }

    // \return whether we should print timing information.
    pub fn wants_timing(&self) -> bool {
        self.properties.wants_timing
    }

    /// \return if we want job control.
    pub fn wants_job_control(&self) -> bool {
        self.group().wants_job_control()
    }

    /// \return whether this job is initially going to run in the background, because & was
    /// specified.
    pub fn is_initially_background(&self) -> bool {
        self.properties.initial_background
    }

    /// Mark this job as constructed. The job must not have previously been marked as constructed.
    pub fn mark_constructed(&mut self) {
        assert!(!self.is_constructed(), "Job was already constructed");
        self.mut_flags().constructed = true;
    }

    /// \return whether we have internal or external procs, respectively.
    /// Internal procs are builtins, blocks, and functions.
    /// External procs include exec and external.
    pub fn has_external_proc(&self) -> bool {
        self.processes.iter().any(|p| !p.is_internal())
    }

    /// \return whether this job, when run, will want a job ID.
    /// Jobs that are only a single internal block do not get a job ID.
    pub fn wants_job_id(&self) -> bool {
        self.processes.len() > 1
            || !self.processes[0].is_internal()
            || self.is_initially_background()
    }

    // Helper functions to check presence of flags on instances of jobs
    /// The job has been fully constructed, i.e. all its member processes have been launched
    pub fn is_constructed(&self) -> bool {
        self.flags().constructed
    }
    /// The job is complete, i.e. all its member processes have been reaped
    /// Return true if all processes in the job have completed.
    pub fn is_completed(&self) -> bool {
        assert!(!self.processes.is_empty());
        for p in &self.processes {
            if !p.completed {
                return false;
            }
        }
        true
    }
    /// The job is in a stopped state
    /// Return true if all processes in the job are stopped or completed, and there is at least one
    /// stopped process.
    pub fn is_stopped(&self) -> bool {
        let mut has_stopped = false;
        for p in &self.processes {
            if !p.completed && !p.stopped {
                return false;
            }
            has_stopped |= p.stopped;
        }
        has_stopped
    }
    /// The job is OK to be externally visible, e.g. to the user via `jobs`
    pub fn is_visible(&self) -> bool {
        !self.is_completed() && self.is_constructed() && !self.flags().disown_requested
    }
    pub fn skip_notification(&self) -> bool {
        self.properties.skip_notification
    }
    #[allow(clippy::wrong_self_convention)]
    pub fn from_event_handler(&self) -> bool {
        self.properties.from_event_handler
    }

    /// \return whether this job's group is in the foreground.
    pub fn is_foreground(&self) -> bool {
        self.group().is_foreground()
    }

    /// \return whether we should post job_exit events.
    pub fn posts_job_exit_events(&self) -> bool {
        // Only report root job exits.
        // For example in `ls | begin ; cat ; end` we don't need to report the cat sub-job.
        if !self.flags().is_group_root {
            return false;
        }

        // Only jobs with external processes post job_exit events.
        self.has_external_proc()
    }

    /// Run ourselves. Returning once we complete or stop.
    pub fn continue_job(&self, parser: &mut Parser) {
        FLOGF!(
            proc_job_run,
            "Run job %d (%ls), %ls, %ls",
            self.job_id().to_wstring(),
            self.command(),
            if self.is_completed() {
                "COMPLETED"
            } else {
                "UNCOMPLETED"
            },
            if parser.libdata().pods.is_interactive {
                "INTERACTIVE"
            } else {
                "NON-INTERACTIVE"
            }
        );

        // Wait for the status of our own job to change.
        while !fish_is_unwinding_for_exit() && !self.is_stopped() && !self.is_completed() {
            process_mark_finished_children(parser, true);
        }
        if self.is_completed() {
            // Set $status only if we are in the foreground and the last process in the job has
            // finished.
            let p = self.processes.last().unwrap();
            if p.status.normal_exited() || p.status.signal_exited() {
                if let Some(statuses) = self.get_statuses() {
                    parser.set_last_statuses(statuses);
                    parser.libdata_mut().pods.status_count += 1;
                }
            }
        }
    }

    /// Prepare to resume a stopped job by sending SIGCONT and clearing the stopped flag.
    /// \return true on success, false if we failed to send the signal.
    pub fn resume(&mut self) -> bool {
        self.mut_flags().notified_of_stop = false;
        if !self.signal(SIGCONT) {
            FLOGF!(
                proc_pgroup,
                "Failed to send SIGCONT to procs in job %ls",
                self.command()
            );
            return false;
        }

        // reset the status of each process instance
        for p in &mut self.processes {
            p.stopped = false;
        }
        true
    }

    /// Send the specified signal to all processes in this job.
    /// \return true on success, false on failure.
    pub fn signal(&self, signal: i32) -> bool {
        if let Some(pgid) = self.group().get_pgid() {
            if unsafe { libc::killpg(pgid, signal) } == -1 {
                let strsignal = unsafe { CString::from_raw(libc::strsignal(signal)) };
                wperror(&sprintf!("killpg(%d, %s)", pgid));
            }
        } else {
            // This job lives in fish's pgroup and we need to signal procs individually.
            for p in &self.processes {
                if !p.completed && p.pid != 0 && unsafe { libc::kill(p.pid, signal) } == -1 {
                    return false;
                }
            }
        }
        true
    }

    /// \returns the statuses for this job.
    pub fn get_statuses(&self) -> Option<Statuses> {
        let mut st = Statuses::default();
        let mut has_status = false;
        let mut laststatus = 0;
        st.pipestatus.reserve(self.processes.len());
        for p in &self.processes {
            let status = p.status;
            if status.is_empty() {
                // Corner case for if a variable assignment is part of a pipeline.
                // e.g. `false | set foo bar | true` will push 1 in the second spot,
                // for a complete pipestatus of `1 1 0`.
                st.pipestatus.push(laststatus);
                continue;
            }
            if status.signal_exited() {
                st.kill_signal = Some(Signal::new(status.signal_code()));
            }
            laststatus = status.status_value();
            has_status = true;
            st.pipestatus.push(status.status_value());
        }
        if !has_status {
            return None;
        }
        st.status = if self.flags().negate {
            !laststatus
        } else {
            laststatus
        };
        Some(st)
    }

    /// \returns the list of processes.
    pub fn get_processes(&self) -> &ProcessList {
        &self.processes
    }
}

pub type JobRef = Arc<RwLock<Job>>; // todo!() no RwLock

/// Whether this shell is attached to a tty.
pub fn is_interactive_session() -> bool {
    IS_INTERACTIVE_SESSION.load()
}
pub fn set_interactive_session(flag: bool) {
    IS_INTERACTIVE_SESSION.store(flag)
}
static IS_INTERACTIVE_SESSION: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Whether we are a login shell.
pub fn get_login() -> bool {
    IS_LOGIN.load()
}
pub fn mark_login() {
    IS_LOGIN.store(true)
}
static IS_LOGIN: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// If this flag is set, fish will never fork or run execve. It is used to put fish into a syntax
/// verifier mode where fish tries to validate the syntax of a file but doesn't actually do
/// anything.
pub fn no_exec() -> bool {
    IS_NO_EXEC.load()
}
pub fn mark_no_exec() {
    IS_NO_EXEC.store(true)
}
static IS_NO_EXEC: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

// List of jobs.
pub type JobList = Vec<JobRef>;

/// The current job control mode.
///
/// Must be one of job_control_t::all, job_control_t::interactive and job_control_t::none.
pub fn get_job_control_mode() -> JobControl {
    unsafe { std::mem::transmute(JOB_CONTROL_MODE.load(Ordering::Relaxed)) }
}
pub fn set_job_control_mode(mode: JobControl) {
    JOB_CONTROL_MODE.store(mode as u8, Ordering::Relaxed);

    // HACK: when fish (or any shell) launches a job with job control, it will put the job into its
    // own pgroup and call tcsetpgrp() to allow that pgroup to own the terminal (making fish a
    // background process). When the job finishes, fish will try to reclaim the terminal via
    // tcsetpgrp(), but as fish is now a background process it will receive SIGTTOU and stop! Ensure
    // that doesn't happen by ignoring SIGTTOU.
    // Note that if we become interactive, we also ignore SIGTTOU.
    if mode == JobControl::all {
        unsafe {
            libc::signal(SIGTTOU, SIG_IGN);
        }
    }
}
static JOB_CONTROL_MODE: AtomicU8 = AtomicU8::new(JobControl::interactive as u8);

/// Notify the user about stopped or terminated jobs, and delete completed jobs from the job list.
/// If \p interactive is set, allow removing interactive jobs; otherwise skip them.
/// \return whether text was printed to stdout.
pub fn job_reap(parser: &mut Parser, allow_interactive: bool) -> bool {
    parser.assert_can_execute();

    // Early out for the common case that there are no jobs.
    if parser.jobs().is_empty() {
        return false;
    }

    process_mark_finished_children(parser, false /* not block_ok */);
    process_clean_after_marking(parser, allow_interactive)
}

/// \return the list of background jobs which we should warn the user about, if the user attempts to
/// exit. An empty result (common) means no such jobs.
pub fn jobs_requiring_warning_on_exit(parser: &Parser) -> JobList {
    let mut result = vec![];
    for job in parser.jobs() {
        if !job.read().unwrap().is_foreground()
            && job.read().unwrap().is_constructed()
            && !job.read().unwrap().is_completed()
        {
            result.push(job.clone());
        }
    }
    result
}

/// Print the exit warning for the given jobs, which should have been obtained via
/// jobs_requiring_warning_on_exit().
#[widestrs]
pub fn print_exit_warning_for_jobs(jobs: &JobList) {
    fputws(wgettext!("There are still jobs active:\n"), STDOUT_FILENO);
    fputws(wgettext!("\n   PID  Command\n"), STDOUT_FILENO);
    for j in jobs {
        fwprintf!(
            STDOUT_FILENO,
            "%6d  %ls\n",
            j.read().unwrap().processes[0].pid,
            j.read().unwrap().command()
        );
    }
    fputws("\n"L, STDOUT_FILENO);
    fputws(
        wgettext!("A second attempt to exit will terminate them.\n"),
        STDOUT_FILENO,
    );
    fputws(
        wgettext!("Use 'disown PID' to remove jobs from the list without terminating them.\n"),
        STDOUT_FILENO,
    );
    reader_schedule_prompt_repaint();
}

/// Use the procfs filesystem to look up how many jiffies of cpu time was used by a given pid. This
/// function is only available on systems with the procfs file entry 'stat', i.e. Linux.
pub fn proc_get_jiffies(inpid: libc::pid_t) -> ClockTicks {
    if inpid <= 0 || !have_proc_stat() {
        return 0;
    }

    let filename = format!("/proc/{}/stat", inpid);
    let Ok(mut f) = fs::File::open(&filename) else {
        return 0;
    };

    let mut buf = vec![];
    if f.read_to_end(&mut buf).is_err() {
        return 0;
    }

    let mut timesstrs = buf.split(|c| *c == b' ').skip(13);
    let mut sum = 0;
    for _ in 0..4 {
        let Some(timestr) = timesstrs.next() else { return 0;};
        let Ok(timestr) = std::str::from_utf8(timestr) else { return 0;};
        let Ok(time) = str::parse::<u64>(timestr) else { return 0;};
        sum += time;
    }
    sum
}

/// Update process time usage for all processes by calling the proc_get_jiffies function for every
/// process of every job.
pub fn proc_update_jiffies(parser: &mut Parser) {
    for job in parser.jobs_mut() {
        for p in &mut job.write().unwrap().processes {
            p.last_time = timef();
            p.last_jiffies = proc_get_jiffies(p.pid);
        }
    }
}

/// Initializations.
pub fn proc_init() {
    signal_set_handlers_once(false);
}

/// Set the status of \p proc to \p status.
fn handle_child_status(job: &Job, proc: &mut Process, status: ProcStatus) {
    proc.status = status;
    if status.stopped() {
        proc.stopped = true;
    } else if status.continued() {
        proc.stopped = false;
    } else {
        proc.completed = true;
    }

    // If the child was killed by SIGINT or SIGQUIT, then cancel the entire group if interactive. If
    // not interactive, we have historically re-sent the signal to ourselves; however don't do that
    // if the signal is trapped (#6649).
    // Note the asymmetry: if the fish process gets SIGINT we will run SIGINT handlers. If a child
    // process gets SIGINT we do not run SIGINT handlers; we just don't exit. This should be
    // rationalized.
    if status.signal_exited() {
        let sig = status.signal_code();
        if [SIGINT, SIGQUIT].contains(&sig) {
            if is_interactive_session() {
                // Mark the job group as cancelled.
                job.group().cancel_with_signal(Signal::new(sig));
            } else if !event::is_signal_observed(sig) {
                // Deliver the SIGINT or SIGQUIT signal to ourself since we're not interactive.
                let mut act: libc::sigaction = unsafe { std::mem::zeroed() };
                unsafe { libc::sigemptyset(&mut act.sa_mask) };
                act.sa_flags = 0;
                act.sa_sigaction = SIG_DFL;
                unsafe {
                    libc::sigaction(sig, &act, std::ptr::null_mut());
                    libc::kill(libc::getpid(), sig);
                }
            }
        }
    }
}

/// Wait for any process finishing, or receipt of a signal.
pub fn proc_wait_any(parser: &mut Parser) {
    process_mark_finished_children(parser, true /*block_ok*/);
    process_clean_after_marking(parser, parser.libdata().pods.is_interactive);
}

/// Send SIGHUP to the list \p jobs, excepting those which are in fish's pgroup.
pub fn hup_jobs(jobs: &JobList) {
    let fish_pgrp = unsafe { libc::getpgrp() };
    for jptr in jobs {
        let j = jptr.read().unwrap();
        let Some(pgid) = j.get_pgid() else {continue};
        if pgid != fish_pgrp && !j.is_completed() {
            if j.is_stopped() {
                j.signal(SIGCONT);
            }
            j.signal(SIGHUP);
        }
    }
}

/// Add a job to the list of PIDs/PGIDs we wait on even though they are not associated with any
/// jobs. Used to avoid zombie processes after disown.
pub fn add_disowned_job(j: &Job) {
    let mut disowned_pids = unsafe { DISOWNED_PIDS.lock().unwrap() };
    for process in &j.processes {
        if process.pid != 0 {
            disowned_pids.push(process.pid);
        }
    }
}

// Reap any pids in our disowned list that have exited. This is used to avoid zombies.
fn reap_disowned_pids() {
    let mut disowned_pids = unsafe { DISOWNED_PIDS.lock().unwrap() };
    // waitpid returns 0 iff the PID/PGID in question has not changed state; remove the pid/pgid
    // if it has changed or an error occurs (presumably ECHILD because the child does not exist)
    disowned_pids.retain(|pid| {
        let mut status: libc::c_int = 0;
        let ret = unsafe { libc::waitpid(*pid, &mut status, WNOHANG) };
        if ret > 0 {
            FLOGF!(proc_reap_external, "Reaped disowned PID or PGID %d", pid);
        }
        ret == 0
    });
}

/// A list of pids that have been disowned. They are kept around until either they exit or
/// we exit. Poll these from time-to-time to prevent zombie processes from happening (#5342).
static mut DISOWNED_PIDS: Mutex<Vec<libc::pid_t>> = Mutex::new(vec![]);

/// See if any reapable processes have exited, and mark them accordingly.
/// \param block_ok if no reapable processes have exited, block until one is (or until we receive a
/// signal).
fn process_mark_finished_children(parser: &mut Parser, block_ok: bool) {
    parser.assert_can_execute();

    // Get the exit and signal generations of all reapable processes.
    // The exit generation tells us if we have an exit; the signal generation allows for detecting
    // SIGHUP and SIGINT.
    // Go through each process and figure out if and how it wants to be reaped.
    let mut reapgens = invalid_generations();
    for j in parser.jobs() {
        for proc in &j.read().unwrap().processes {
            if !j.read().unwrap().can_reap(proc) {
                continue;
            }

            if proc.pid > 0 {
                // Reaps with a pid.
                reapgens.set_min_from(topic_t::sigchld, &proc.gens);
                reapgens.set_min_from(topic_t::sighupint, &proc.gens);
            }
            if proc.internal_proc.is_some() {
                // Reaps with an internal process.
                reapgens.set_min_from(topic_t::internal_exit, &proc.gens);
                reapgens.set_min_from(topic_t::sighupint, &proc.gens);
            }
        }
    }

    // Now check for changes, optionally waiting.
    if !topic_monitor_principal().check(&mut reapgens, block_ok) {
        // Nothing changed.
        return;
    }

    // We got some changes. Since we last checked we received SIGCHLD, and or HUP/INT.
    // Update the hup/int generations and reap any reapable processes.
    // We structure this as two loops for some simplicity.
    // First reap all pids.
    for j in parser.jobs_mut() {
        for proc in &mut j.write().unwrap().processes {
            // Does this proc have a pid that is reapable?
            if proc.pid <= 0 || !j.read().unwrap().can_reap(proc) {
                continue;
            }

            // Always update the signal hup/int gen.
            proc.gens.sighupint = reapgens.sighupint;

            // Nothing to do if we did not get a new sigchld.
            if proc.gens.sigchld == reapgens.sigchld {
                continue;
            }
            proc.gens.sigchld = reapgens.sigchld;

            // Ok, we are reapable. Run waitpid()!
            let mut statusv: libc::c_int = -1;
            let pid =
                unsafe { libc::waitpid(proc.pid, &mut statusv, WNOHANG | WUNTRACED | WCONTINUED) };
            assert!(pid <= 0 || pid == proc.pid, "Unexpcted waitpid() return");
            if pid <= 0 {
                continue;
            }

            // The process has stopped or exited! Update its status.
            let status = ProcStatus::from_waitpid(statusv);
            handle_child_status(&j.read().unwrap(), &mut *proc, status);
            if status.stopped() {
                j.write().unwrap().group().set_is_foreground(false);
            }
            if status.continued() {
                j.write().unwrap().mut_flags().notified_of_stop = false;
            }
            if status.normal_exited() || status.signal_exited() {
                FLOGF!(
                    proc_reap_external,
                    "Reaped external process '%ls' (pid %d, status %d)",
                    proc.argv0().unwrap(),
                    pid,
                    proc.status.status_value()
                );
            } else {
                assert!(status.stopped() || status.continued());
                FLOGF!(
                    proc_reap_external,
                    "External process '%ls' (pid %d, %s)",
                    proc.argv0().unwrap(),
                    proc.pid,
                    if proc.status.stopped() {
                        "stopped"
                    } else {
                        "continued"
                    }
                );
            }
        }
    }

    // We are done reaping pids.
    // Reap internal processes.
    for j in parser.jobs_mut() {
        for proc in &mut j.write().unwrap().processes {
            // Does this proc have an internal process that is reapable?
            if proc.internal_proc.is_none() || !j.read().unwrap().can_reap(proc) {
                continue;
            }

            // Always update the signal hup/int gen.
            proc.gens.sighupint = reapgens.sighupint;

            // Nothing to do if we did not get a new internal exit.
            if proc.gens.internal_exit == reapgens.internal_exit {
                continue;
            }
            proc.gens.internal_exit = reapgens.internal_exit;

            // Has the process exited?
            if !proc
                .internal_proc
                .as_ref()
                .unwrap()
                .read()
                .unwrap()
                .exited()
            {
                continue;
            }

            // The process gets the status from its internal proc.
            let status = proc
                .internal_proc
                .as_ref()
                .unwrap()
                .read()
                .unwrap()
                .get_status();
            handle_child_status(&j.read().unwrap(), &mut *proc, status);
            FLOGF!(
                proc_reap_internal,
                "Reaped internal process '%ls' (id %llu, status %d)",
                proc.argv0().unwrap(),
                proc.internal_proc
                    .as_ref()
                    .unwrap()
                    .read()
                    .unwrap()
                    .get_id(),
                proc.status.status_value(),
            );
        }
    }

    // Remove any zombies.
    reap_disowned_pids();
}

/// Generate process_exit events for any completed processes in \p j.
fn generate_process_exit_events(j: &JobRef, out_evts: &mut Vec<Event>) {
    // Historically we have avoided generating events for foreground jobs from event handlers, as an
    // event handler may itself produce a new event.
    if !j.read().unwrap().from_event_handler() || j.read().unwrap().is_foreground() {
        for p in &mut j.write().unwrap().processes {
            if p.pid > 0 && p.completed && !p.posted_proc_exit {
                p.posted_proc_exit = true;
                out_evts.push(Event::process_exit(p.pid, p.status.status_value()));
            }
        }
    }
}

/// Given a job that has completed, generate job_exit and caller_exit events.
fn generate_job_exit_events(j: &Job, out_evts: &mut Vec<Event>) {
    // Generate proc and job exit events, except for foreground jobs originating in event handlers.
    if !j.from_event_handler() || j.is_foreground() {
        // job_exit events.
        if j.posts_job_exit_events() {
            if let Some(last_pid) = j.get_last_pid() {
                out_evts.push(Event::job_exit(last_pid, j.internal_job_id));
            }
        }
    }
    // Generate caller_exit events.
    out_evts.push(Event::caller_exit(j.internal_job_id, j.job_id()));
}

/// \return whether to emit a fish_job_summary call for a process.
fn proc_wants_summary(j: &Job, p: &Process) -> bool {
    // Are we completed with a pid?
    if !p.completed || p.pid == 0 {
        return false;
    }

    // Did we die due to a signal other than SIGPIPE?
    let s = p.status;
    if s.signal_exited() || s.signal_code() == SIGPIPE {
        return false;
    }

    // Does the job want to suppress notifications?
    // Note we always report crashes.
    if j.skip_notification() && !CRASHSIGNALS.contains(&s.signal_code()) {
        return false;
    }

    true
}

/// \return whether to emit a fish_job_summary call for a job as a whole. We may also emit this for
/// its individual processes.
fn job_wants_summary(j: &Job) -> bool {
    // Do we just skip notifications?
    if j.skip_notification() {
        return false;
    }

    // Do we have a single process which will also report? If so then that suffices for us.
    if j.processes.len() == 1 && proc_wants_summary(j, &j.processes[0]) {
        return false;
    }

    // Are we foreground?
    // The idea here is to not print status messages for jobs that execute in the foreground (i.e.
    // without & and without being `bg`).
    if j.is_foreground() {
        return false;
    }

    true
}

/// \return whether we want to emit a fish_job_summary call for a job or any of its processes.
fn job_or_proc_wants_summary(j: &Job) -> bool {
    job_wants_summary(j) || j.processes.iter().any(|p| proc_wants_summary(j, p))
}

/// Invoke the fish_job_summary function by executing the given command.
fn call_job_summary(parser: &mut Parser, cmd: &wstr) {
    let event = Event::generic(L!("fish_job_summary").to_owned());
    let b = parser.push_block(Block::event_block(event));
    let saved_status = parser.get_last_statuses();
    parser.eval(cmd, &IoChain::new());
    parser.set_last_statuses(saved_status);
    parser.pop_block(b);
}

// \return a command which invokes fish_job_summary.
// The process pointer may be null, in which case it represents the entire job.
// Note this implements the arguments which fish_job_summary expects.
#[widestrs]
fn summary_command(j: &Job, p: Option<&Process>) -> WString {
    let mut buffer = "fish_job_summary"L.to_owned();

    // Job id.
    buffer += &sprintf!(" %d", j.job_id().to_wstring())[..];

    // 1 if foreground, 0 if background.
    buffer += &sprintf!(" %d", if j.is_foreground() { 1 } else { 0 })[..];

    // Command.
    buffer.push(' ');
    buffer += &escape(j.command())[..];

    match p {
        None => {
            // No process, we are summarizing the whole job.
            buffer += if j.is_stopped() {
                " STOPPED"L
            } else {
                " ENDED"L
            };
        }
        Some(p) => {
            // We are summarizing a process which exited with a signal.
            // Arguments are the signal name and description.
            let sig = Signal::new(p.status.signal_code());
            buffer.push(' ');
            buffer += &escape(&sig.name())[..];

            buffer.push(' ');
            buffer += &escape(&sig.desc())[..];

            // If we have multiple processes, we also append the pid and argv.
            if j.processes.len() > 1 {
                buffer += &sprintf!(" %d", p.pid)[..];

                buffer.push(' ');
                buffer += &escape(p.argv0().unwrap())[..];
            }
        }
    }
    buffer
}

// Summarize a list of jobs, by emitting calls to fish_job_summary.
// Note the given list must NOT be the parser's own job list, since the call to fish_job_summary
// could modify it.
fn summarize_jobs(parser: &mut Parser, jobs: &[JobRef]) -> bool {
    if jobs.is_empty() {
        return false;
    }

    for j in jobs {
        let j = &*j.read().unwrap();
        if j.is_stopped() {
            call_job_summary(parser, &summary_command(j, None));
        } else {
            // Completed job.
            for p in &j.processes {
                if proc_wants_summary(j, p) {
                    call_job_summary(parser, &summary_command(j, Some(p)));
                }
            }

            // Overall status for the job.
            if job_wants_summary(j) {
                call_job_summary(parser, &summary_command(j, None));
            }
        }
    }
    true
}

/// Remove all disowned jobs whose job chain is fully constructed (that is, do not erase disowned
/// jobs that still have an in-flight parent job). Note we never print statuses for such jobs.
fn remove_disowned_jobs(jobs: &mut JobList) {
    jobs.retain(|j| {
        let j = j.read().unwrap();
        !j.flags().disown_requested || !j.is_constructed()
    });
}

/// Given that a job has completed, check if it may be wait'ed on; if so add it to the wait handle
/// store. Then mark all wait handles as complete.
fn save_wait_handle_for_completed_job(job: &JobRef, store: &mut WaitHandleStore) {
    assert!(job.read().unwrap().is_completed(), "Job not completed");
    // Are we a background job?
    if !job.read().unwrap().is_foreground() {
        for proc in &mut job.write().unwrap().processes {
            if let Some(wh) = proc.make_wait_handle(job.read().unwrap().internal_job_id) {
                store.add(wh);
            }
        }
    }

    // Mark all wait handles as complete (but don't create just for this).
    for proc in &job.read().unwrap().processes {
        if let Some(wh) = proc.get_wait_handle() {
            wh.set_status_and_complete(proc.status.status_value());
        }
    }
}

/// Remove completed jobs from the job list, printing status messages as appropriate.
/// \return whether something was printed.
fn process_clean_after_marking(parser: &mut Parser, allow_interactive: bool) -> bool {
    parser.assert_can_execute();

    // This function may fire an event handler, we do not want to call ourselves recursively (to
    // avoid infinite recursion).
    if parser.libdata().pods.is_cleaning_procs {
        return false;
    }

    let mut parser = scoped_push(
        parser,
        |parser| &mut parser.libdata_mut().pods.is_cleaning_procs,
        true,
    );

    // This may be invoked in an exit handler, after the TERM has been torn down
    // Don't try to print in that case (#3222)
    let interactive = allow_interactive && cur_term();

    // Remove all disowned jobs.
    remove_disowned_jobs(parser.jobs_mut());

    // Accumulate exit events into a new list, which we fire after the list manipulation is
    // complete.
    let mut exit_events = vec![];

    // Defer processing under-construction jobs or jobs that want a message when we are not
    // interactive.
    let should_process_job = |j: &Job| {
        // Do not attempt to process jobs which are not yet constructed.
        // Do not attempt to process jobs that need to print a status message,
        // unless we are interactive, in which case printing is OK.
        j.is_constructed() && (interactive || !job_or_proc_wants_summary(j))
    };

    // The list of jobs to summarize. Some of these jobs are completed and are removed from the
    // parser's job list, others are stopped and remain in the list.
    let mut jobs_to_summarize = vec![];

    // Handle stopped jobs. These stay in our list.
    for jptr in parser.jobs() {
        let j = jptr.read().unwrap();
        if j.is_stopped()
            && !j.flags().notified_of_stop
            && should_process_job(&j)
            && job_wants_summary(&j)
        {
            jptr.write().unwrap().mut_flags().notified_of_stop = true;
            jobs_to_summarize.push(jptr.clone());
        }
    }

    // Generate process_exit events for finished processes.
    for j in parser.jobs() {
        generate_process_exit_events(j, &mut exit_events);
    }

    // Remove completed, processable jobs from our job list.
    let mut completed_jobs = vec![];
    parser.jobs_mut().retain(|jptr| {
        let j = jptr.read().unwrap();
        if !should_process_job(&j) || !j.is_completed() {
            return true;
        }
        // We are committed to removing this job.
        // Remember it for summary later, generate exit events, maybe save its wait handle if it
        // finished in the background.
        if job_or_proc_wants_summary(&j) {
            jobs_to_summarize.push(jptr.clone());
        }
        generate_job_exit_events(&j, &mut exit_events);
        completed_jobs.push(jptr.clone());
        false
    });
    for j in completed_jobs {
        save_wait_handle_for_completed_job(&j, parser.mut_wait_handles());
    }

    // Emit calls to fish_job_summary.
    let printed = summarize_jobs(&mut parser, &jobs_to_summarize);

    // Post pending exit events.
    for evt in exit_events {
        todo!("need to switch to the Rust parser.")
        // event::fire(parser, evt);
    }

    if printed {
        let _ = std::io::stdout().lock().flush();
    }

    printed
}

pub fn have_proc_stat() -> bool {
    // Check for /proc/self/stat to see if we are running with Linux-style procfs.
    static HAVE_PROC_STAT_RESULT: Lazy<bool> =
        Lazy::new(|| fs::metadata("/proc/self/stat").is_ok());
    *HAVE_PROC_STAT_RESULT
}

/// The signals that signify crashes to us.
const CRASHSIGNALS: [libc::c_int; 6] = [SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGSYS];

pub struct JobRefFfi(JobRef);

unsafe impl cxx::ExternType for JobRefFfi {
    type Id = cxx::type_id!("JobRefFfi");
    type Kind = cxx::kind::Opaque;
}

pub struct JobGroupRefFfi(JobGroupRef);

unsafe impl cxx::ExternType for JobGroupRefFfi {
    type Id = cxx::type_id!("JobGroupRefFfi");
    type Kind = cxx::kind::Opaque;
}

#[cxx::bridge]
mod proc_ffi {
    extern "Rust" {
        type JobRefFfi;
        type JobGroupRefFfi;

        fn new_job_group_ref() -> Box<JobGroupRefFfi>;
    }
}

fn new_job_group_ref() -> Box<JobGroupRefFfi> {
    todo!()
    // Box::new(JobGroupRefFfi(JobGroupRef
}
