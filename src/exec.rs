// Functions for executing a program.
//
// Some of the code in this file is based on code from the Glibc manual, though the changes
// performed have been massive.

use crate::builtins::shared::{
    ErrorCode, STATUS_CMD_ERROR, STATUS_CMD_UNKNOWN, STATUS_NOT_EXECUTABLE, STATUS_READ_TOO_MUCH,
    builtin_run,
};
use crate::common::{
    ScopeGuard, bytes2wcstring, exit_without_destructors, truncate_at_nul, wcs2bytes, wcs2zstring,
    write_loop,
};
use crate::env::{EnvMode, EnvStack, Environment, READ_BYTE_LIMIT, Statuses};
#[cfg(FISH_USE_POSIX_SPAWN)]
use crate::env_dispatch::use_posix_spawn;
use crate::fds::make_fd_blocking;
use crate::fds::{PIPE_ERROR, make_autoclose_pipes, open_cloexec};
use crate::flog::{FLOG, FLOGF};
use crate::fork_exec::PATH_BSHELL;
use crate::fork_exec::blocked_signals_for_job;
use crate::fork_exec::postfork::{
    child_setup_process, execute_fork, execute_setpgid, report_setpgid_error,
    safe_report_exec_error,
};
#[cfg(FISH_USE_POSIX_SPAWN)]
use crate::fork_exec::spawn::PosixSpawner;
use crate::function::{self, FunctionProperties};
use crate::io::{
    BufferedOutputStream, FdOutputStream, IoBufferfill, IoChain, IoClose, IoMode, IoPipe,
    IoStreams, OutputStream, SeparatedBuffer, StringOutputStream,
};
use crate::nix::{getpid, isatty};
use crate::null_terminated_array::OwningNullTerminatedArray;
use crate::parser::{Block, BlockId, BlockType, EvalRes, Parser};
#[cfg(FISH_USE_POSIX_SPAWN)]
use crate::proc::Pid;
use crate::proc::{
    InternalProc, Job, JobGroupRef, ProcStatus, Process, ProcessType, hup_jobs,
    is_interactive_session, jobs_requiring_warning_on_exit, no_exec, print_exit_warning_for_jobs,
};
use crate::reader::{reader_run_count, safe_restore_term_mode};
use crate::redirection::{Dup2List, dup2_list_resolve_chain};
use crate::threads::{ThreadPool, is_forked_child};
use crate::trace::trace_if_enabled_with_args;
use crate::tty_handoff::TtyHandoff;
use crate::wchar::prelude::*;
use crate::wchar_ext::ToWString;
use crate::wutil::{fish_wcstol, perror};
use errno::{errno, set_errno};
use libc::{
    EACCES, ENOENT, ENOEXEC, ENOTDIR, EPIPE, EXIT_FAILURE, EXIT_SUCCESS, STDERR_FILENO,
    STDIN_FILENO, STDOUT_FILENO, c_char,
};
use nix::fcntl::OFlag;
use nix::sys::stat;
use std::ffi::CStr;
use std::io::{Read, Write};
use std::mem::MaybeUninit;
use std::num::NonZeroU32;
use std::os::fd::{AsRawFd, OwnedFd, RawFd};
use std::slice;
use std::sync::{
    Arc, OnceLock,
    atomic::{AtomicUsize, Ordering},
};

/// The singleton shared exec thread pool.
/// This is used to write the output of internal processes (e.g. builtins)
/// to their target fds.
/// TODO: this IO could be multiplexed using FdMonitor.
fn exec_thread_pool() -> &'static Arc<ThreadPool> {
    static EXEC_THREAD_POOL: OnceLock<Arc<ThreadPool>> = OnceLock::new();
    // Use an unbounded queue because otherwise we risk deadlock.
    EXEC_THREAD_POOL.get_or_init(|| ThreadPool::new(1, usize::MAX))
}

/// Execute the processes specified by `j` in the parser \p.
/// On a true return, the job was successfully launched and the parser will take responsibility for
/// cleaning it up. On a false return, the job could not be launched and the caller must clean it
/// up.
pub fn exec_job(parser: &Parser, job: &Job, block_io: IoChain) -> bool {
    // If fish was invoked with -n or --no-execute, then no_exec will be set and we do nothing.
    if no_exec() {
        return true;
    }

    // Handle an exec call.
    if job.processes()[0].is_exec() {
        // If we are interactive, perhaps disallow exec if there are background jobs.
        if !allow_exec_with_background_jobs(parser) {
            for p in job.processes().iter() {
                p.mark_aborted_before_launch();
            }
            return false;
        }

        // Apply foo=bar variable assignments
        for assignment in &job.processes()[0].variable_assignments {
            parser.vars().set(
                &assignment.variable_name,
                EnvMode::LOCAL | EnvMode::EXPORT,
                assignment.values.clone(),
            );
        }

        internal_exec(parser.vars(), job, block_io);
        // internal_exec only returns if it failed to set up redirections.
        // In case of an successful exec, this code is not reached.
        let status = if job.flags().negate { 0 } else { 1 };
        parser.set_last_statuses(Statuses::just(status));

        // A false return tells the caller to remove the job from the list.
        for p in job.processes().iter() {
            p.mark_aborted_before_launch();
        }
        return false;
    }

    // Get the deferred process, if any. We will have to remember its pipes.
    let mut deferred_pipes = PartialPipes::default();
    let deferred_process = get_deferred_process(job);

    // We may want to transfer tty ownership to the pgroup leader.
    let mut handoff = TtyHandoff::new(|| {});

    // This loop loops over every process_t in the job, starting it as appropriate. This turns out
    // to be rather complex, since a process_t can be one of many rather different things.
    //
    // The loop also has to handle pipelining between the jobs.
    //
    // We can have up to three pipes "in flight" at a time:
    //
    // 1. The pipe the current process should read from (courtesy of the previous process)
    // 2. The pipe that the current process should write to
    // 3. The pipe that the next process should read from (courtesy of us)
    //
    // Lastly, a process may experience a pipeline-aborting error, which prevents launching
    // further processes in the pipeline.
    let mut pipe_next_read: Option<OwnedFd> = None;
    let mut aborted_pipeline = false;
    let mut procs_launched = 0;
    for i in 0..job.processes().len() {
        let p = &job.processes()[i];
        // proc_pipes is the pipes applied to this process. That is, it is the read end
        // containing the output of the previous process (if any), plus the write end that will
        // output to the next process (if any).
        let mut proc_pipes = PartialPipes::default();
        std::mem::swap(&mut proc_pipes.read, &mut pipe_next_read);
        if !p.is_last_in_job {
            let Ok(pipes) = make_autoclose_pipes() else {
                FLOG!(warning, wgettext!(PIPE_ERROR));
                aborted_pipeline = true;
                abort_pipeline_from(job, i);
                break;
            };
            pipe_next_read = Some(pipes.read);
            proc_pipes.write = Some(pipes.write);

            // Save any deferred process for last. By definition, the deferred process can
            // never be the last process in the job, so it's safe to nest this in the outer
            // `if (!p->is_last_in_job)` block, which makes it clear that `proc_next_read` will
            // always be assigned when we `continue` the loop.
            if Some(i) == deferred_process {
                deferred_pipes = proc_pipes;
                continue;
            }
        }

        // Regular process.
        if exec_process_in_job(
            parser,
            p,
            job,
            block_io.clone(),
            proc_pipes,
            &deferred_pipes,
            false,
        )
        .is_err()
        {
            aborted_pipeline = true;
            abort_pipeline_from(job, i);
            break;
        }
        procs_launched += 1;

        // Transfer tty?
        if p.leads_pgrp && job.group().wants_terminal() {
            handoff.to_job_group(job.group.as_ref().unwrap());
        }
    }
    drop(pipe_next_read);

    // If our pipeline was aborted before any process was successfully launched, then there is
    // nothing to reap, and we can perform an early return.
    // Note we must never return false if we have launched even one process, since it will not be
    // properly reaped; see #7038.
    if aborted_pipeline && procs_launched == 0 {
        return false;
    }

    // Ok, at least one thing got launched.
    // Handle any deferred process.
    if let Some(dp) = deferred_process {
        if
        // Some other process already aborted our pipeline.
        aborted_pipeline
            // The deferred proc itself failed to launch.
            || exec_process_in_job(
                parser,
                &job.processes()[dp],
                job,
                block_io,
                deferred_pipes,
                &PartialPipes::default(),
                true,
            )
            .is_err()
        {
            job.processes()[dp].mark_aborted_before_launch();
        }
    }

    FLOGF!(
        exec_job_exec,
        "Executed job %d from command '%s'",
        job.job_id(),
        job.command()
    );

    job.mark_constructed();

    // If exec_error then a backgrounded job would have been terminated before it was ever assigned
    // a pgroup, so error out before setting last_pid.
    if !job.is_foreground() {
        if let Some(last_pid) = job.get_last_pid() {
            parser
                .vars()
                .set_one(L!("last_pid"), EnvMode::GLOBAL, last_pid.to_wstring());
        } else {
            parser.vars().set_empty(L!("last_pid"), EnvMode::GLOBAL);
        }
    }

    if !job.is_initially_background() {
        job.continue_job(parser);
    }

    if job.is_stopped() {
        handoff.save_tty_modes();
    }
    handoff.reclaim();
    true
}

/// Evaluate a command.
///
/// \param cmd the command to execute
/// \param parser the parser with which to execute code
/// \param outputs if set, the list to insert output into.
/// \param apply_exit_status if set, update $status within the parser, otherwise do not.
///
/// Return a value appropriate for populating $status.
pub fn exec_subshell(
    cmd: &wstr,
    parser: &Parser,
    outputs: Option<&mut Vec<WString>>,
    apply_exit_status: bool,
) -> Result<(), ErrorCode> {
    let mut break_expand = false;
    exec_subshell_internal(
        cmd,
        parser,
        None,
        outputs,
        &mut break_expand,
        apply_exit_status,
        false,
    )
}

/// Like exec_subshell, but only returns expansion-breaking errors. That is, a zero return means
/// "success" (even though the command may have failed), a non-zero return means that we should
/// halt expansion. If the `pgid` is supplied, then any spawned external commands should join that
/// pgroup.
pub fn exec_subshell_for_expand(
    cmd: &wstr,
    parser: &Parser,
    job_group: Option<&JobGroupRef>,
    outputs: &mut Vec<WString>,
) -> Result<(), ErrorCode> {
    let mut break_expand = true;
    let ret = exec_subshell_internal(
        cmd,
        parser,
        job_group,
        Some(outputs),
        &mut break_expand,
        true,
        true,
    );
    // Only return an error code if we should break expansion.
    if break_expand { ret } else { Ok(()) }
}

/// Number of calls to fork() or posix_spawn().
static FORK_COUNT: AtomicUsize = AtomicUsize::new(0);

/// A launch_result_t indicates when a process failed to launch, and therefore the rest of the
/// pipeline should be aborted. This includes failed redirections, fd exhaustion, fork() failures,
/// etc.
type LaunchResult = Result<(), ()>;

/// Given an error `err` returned from either posix_spawn or exec, Return a process exit code.
fn exit_code_from_exec_error(err: libc::c_int) -> libc::c_int {
    assert!(err != 0, "Zero is success, not an error");
    match err {
        ENOENT | ENOTDIR => {
            // This indicates either the command was not found, or a file redirection was not found.
            // We do not use posix_spawn file redirections so this is always command-not-found.
            STATUS_CMD_UNKNOWN
        }
        EACCES | ENOEXEC => {
            // The file is not executable for various reasons.
            STATUS_NOT_EXECUTABLE
        }
        #[cfg(apple)]
        libc::EBADARCH => {
            // This is for e.g. running ARM app on Intel Mac.
            STATUS_NOT_EXECUTABLE
        }
        _ => {
            // Generic failure.
            EXIT_FAILURE
        }
    }
}

/// This is a 'looks like text' check.
/// Return true if either there is no NUL byte, or there is a line containing a lowercase letter
/// before the first NUL byte.
fn is_thompson_shell_payload(p: &[u8]) -> bool {
    if !p.contains(&b'\0') {
        return true;
    };
    let mut haslower = false;
    for c in p {
        if c.is_ascii_lowercase() || *c == b'$' || *c == b'`' {
            haslower = true;
        }
        if haslower && *c == b'\n' {
            return true;
        }
    }
    false
}

/// This function checks the beginning of a file to see if it's safe to
/// pass to the system interpreter when execve() returns ENOEXEC.
///
/// The motivation is to be able to run classic shell scripts which
/// didn't have shebang, while protecting the user from accidentally
/// running a binary file which may corrupt terminal driver state. We
/// check for lowercase letters because the ASCII magic of binary files
/// is usually uppercase, e.g. PNG, JFIF, MZ, etc. These rules are also
/// flexible enough to permit scripts with concatenated binary content,
/// such as Actually Portable Executable.
/// N.B.: this is called after fork, it must not allocate heap memory.
pub fn is_thompson_shell_script(path: &CStr) -> bool {
    // Paths ending in ".fish" are never considered Thompson shell scripts.
    if path.to_bytes().ends_with(".fish".as_bytes()) {
        return false;
    }
    let e = errno();
    let mut res = false;
    if let Ok(mut file) = open_cloexec(path, OFlag::O_RDONLY | OFlag::O_NOCTTY, stat::Mode::empty())
    {
        let mut buf = [b'\0'; 256];
        if let Ok(got) = file.read(&mut buf) {
            if is_thompson_shell_payload(&buf[..got]) {
                res = true;
            }
        }
    }
    set_errno(e);
    res
}

/// This function is executed by the child process created by a call to fork(). It should be called
/// after \c child_setup_process. It calls execve to replace the fish process image with the command
/// specified in \c p. It never returns. Called in a forked child! Do not allocate memory, etc.
fn safe_launch_process(
    _p: &Process,
    actual_cmd: &CStr,
    argv: &OwningNullTerminatedArray,
    envv: &OwningNullTerminatedArray,
) -> ! {
    // This function never returns, so we take certain liberties with constness.

    unsafe { libc::execve(actual_cmd.as_ptr(), argv.get(), envv.get()) };
    let err = errno();

    // The shebang wasn't introduced until UNIX Seventh Edition, so if
    // the kernel won't run the binary we hand it off to the interpreter
    // after performing a binary safety check, recommended by POSIX: a
    // line needs to exist before the first \0 with a lowercase letter

    if err.0 == ENOEXEC && is_thompson_shell_script(actual_cmd) {
        // Construct new argv.
        // We must not allocate memory, so only 128 args are supported.
        const MAXARGS: usize = 128;
        let nargs = argv.len();
        let argv = unsafe { slice::from_raw_parts(argv.get(), nargs) };
        if nargs <= MAXARGS {
            // +1 for /bin/sh, +1 for terminating nullptr
            let mut argv2 = [std::ptr::null(); 1 + MAXARGS + 1];
            let bshell = PATH_BSHELL.as_ptr() as *const c_char;
            argv2[0] = bshell as *mut c_char;
            argv2[1..argv.len() + 1].copy_from_slice(argv);
            // The command to call should use the full path,
            // not what we would pass as argv0.
            argv2[1] = actual_cmd.as_ptr();
            unsafe {
                libc::execve(bshell, &argv2[0], envv.get());
            }
        }
    }

    set_errno(err);
    safe_report_exec_error(errno().0, actual_cmd, argv, envv);
    exit_without_destructors(exit_code_from_exec_error(err.0));
}

/// This function is similar to launch_process, except it is not called after a fork (i.e. it only
/// calls exec) and therefore it can allocate memory.
fn launch_process_nofork(vars: &EnvStack, p: &Process) -> ! {
    assert!(!is_forked_child());

    // Construct argv. Ensure the strings stay alive for the duration of this function.
    let narrow_strings = p.argv().iter().map(|s| wcs2zstring(s)).collect();
    let argv = OwningNullTerminatedArray::new(narrow_strings);

    // Construct envp.
    let envp = vars.export_array();
    let actual_cmd = wcs2zstring(&p.actual_cmd);

    // Ensure the terminal modes are what they were before we changed them.
    safe_restore_term_mode();
    // Bounce to launch_process. This never returns.
    safe_launch_process(p, &actual_cmd, &argv, &envp);
}

// Returns whether we can use posix spawn for a given process in a given job.
//
// To avoid the race between the caller calling tcsetpgrp() and the client checking the
// foreground process group, we don't use posix_spawn if we're going to foreground the process. (If
// we use fork(), we can call tcsetpgrp after the fork, before the exec, and avoid the race).
#[cfg(FISH_USE_POSIX_SPAWN)]
fn can_use_posix_spawn_for_job(job: &Job, dup2s: &Dup2List) -> bool {
    // Is it globally disabled?
    if !use_posix_spawn() {
        return false;
    }

    // Hack - do not use posix_spawn if there are self-fd redirections.
    // For example if you were to write:
    //   cmd 6< /dev/null
    // it is possible that the open() of /dev/null would result in fd 6. Here even if we attempted
    // to add a dup2 action, it would be ignored and the CLO_EXEC bit would remain. So don't use
    // posix_spawn in this case; instead we'll call fork() and clear the CLO_EXEC bit manually.
    for action in dup2s.get_actions() {
        if action.src == action.target {
            return false;
        }
    }
    // If this job will be foregrounded, we will call tcsetpgrp(), therefore do not use
    // posix_spawn.
    let wants_terminal = job.group().wants_terminal();
    !wants_terminal
}

fn internal_exec(vars: &EnvStack, j: &Job, block_io: IoChain) {
    // Do a regular launch -  but without forking first...
    let mut all_ios = block_io;
    if !all_ios.append_from_specs(j.processes()[0].redirection_specs(), &vars.get_pwd_slash()) {
        return;
    }

    let mut blocked_signals = MaybeUninit::uninit();
    let mut blocked_signals = unsafe {
        libc::sigemptyset(blocked_signals.as_mut_ptr());
        blocked_signals.assume_init()
    };
    let blocked_signals = if blocked_signals_for_job(j, &mut blocked_signals) {
        Some(&blocked_signals)
    } else {
        None
    };

    // child_setup_process makes sure signals are properly set up.
    let redirs = dup2_list_resolve_chain(&all_ios);
    if child_setup_process(
        None, /* not claim_tty */
        blocked_signals,
        false, /* not is_forked */
        &redirs,
    ) == 0
    {
        // Decrement SHLVL as we're removing ourselves from the shell "stack".
        if is_interactive_session() {
            let shlvl_var = vars.getf(L!("SHLVL"), EnvMode::GLOBAL | EnvMode::EXPORT);
            let mut shlvl_str = L!("0").to_owned();
            if let Some(shlvl_var) = shlvl_var {
                if let Ok(shlvl) = fish_wcstol(&shlvl_var.as_string()) {
                    if shlvl > 0 {
                        shlvl_str = (shlvl - 1).to_wstring();
                    }
                }
            }
            vars.set_one(L!("SHLVL"), EnvMode::GLOBAL | EnvMode::EXPORT, shlvl_str);
        }

        // launch_process _never_ returns.
        launch_process_nofork(vars, &j.processes()[0]);
    }
}

/// Construct an internal process for the process p. In the background, write the data `outdata` to
/// stdout and `errdata` to stderr, respecting the io chain `ios`. For example if target_fd is 1
/// (stdout), and there is a dup2 3->1, then we need to write to fd 3. Then exit the internal
/// process.
fn run_internal_process(p: &Process, outdata: Vec<u8>, errdata: Vec<u8>, ios: &IoChain) {
    p.check_generations_before_launch();

    // We want both the dup2s and the io_chain_ts to be kept alive by the background thread, because
    // they may own an fd that we want to write to. Move them all to a shared_ptr. The strings as
    // well (they may be long).
    // Construct a little helper struct to make it simpler to move into our closure without copying.
    struct WriteFields {
        src_outfd: RawFd,
        outdata: Vec<u8>,

        src_errfd: RawFd,
        errdata: Vec<u8>,

        ios: IoChain,
        dup2s: Dup2List,
        internal_proc: Arc<InternalProc>,

        success_status: ProcStatus,
    }
    impl WriteFields {
        fn skip_out(&self) -> bool {
            self.outdata.is_empty() || self.src_outfd < 0
        }
        fn skip_err(&self) -> bool {
            self.errdata.is_empty() || self.src_errfd < 0
        }
    }

    // Construct and assign the internal process to the real process.
    let internal_proc = Arc::new(InternalProc::new());
    let old = p.internal_proc.replace(Some(internal_proc.clone()));
    assert!(
        old.is_none(),
        "Replaced p.internal_proc, but it already had a value!"
    );
    let mut f = Box::new(WriteFields {
        src_outfd: -1,
        outdata,

        src_errfd: -1,
        errdata,

        ios: IoChain::default(),
        dup2s: Dup2List::new(),
        internal_proc: internal_proc.clone(),

        success_status: ProcStatus::default(),
    });

    FLOGF!(
        proc_internal_proc,
        "Created internal proc %u to write output for proc '%s'",
        internal_proc.get_id(),
        p.argv0().unwrap()
    );

    // Resolve the IO chain.
    // Note it's important we do this even if we have no out or err data, because we may have been
    // asked to truncate a file (e.g. `echo -n '' > /tmp/truncateme.txt'). The open() in the dup2
    // list resolution will ensure this happens.
    f.dup2s = dup2_list_resolve_chain(ios);

    // Figure out which source fds to write to. If they are closed (unlikely) we just exit
    // successfully.
    f.src_outfd = f.dup2s.fd_for_target_fd(STDOUT_FILENO);
    f.src_errfd = f.dup2s.fd_for_target_fd(STDERR_FILENO);

    // If we have nothing to write we can elide the thread.
    // TODO: support eliding output to /dev/null.
    if f.skip_out() && f.skip_err() {
        internal_proc.mark_exited(p.status());
        return;
    }

    // Ensure that ios stays alive, it may own fds.
    f.ios = ios.clone();

    // If our process is a builtin, it will have already set its status value. Make sure we
    // propagate that if our I/O succeeds and don't read it on a background thread. TODO: have
    // builtin_run provide this directly, rather than setting it in the process.
    f.success_status = p.status();

    exec_thread_pool().perform(move || {
        let mut status = f.success_status;
        if !f.skip_out() {
            if let Err(err) = write_loop(&f.src_outfd, &f.outdata) {
                if err.raw_os_error() != Some(EPIPE) {
                    perror("write");
                }
                if status.is_success() {
                    status = ProcStatus::from_exit_code(1);
                }
            }
        }
        if !f.skip_err() {
            if let Err(err) = write_loop(&f.src_errfd, &f.errdata) {
                if err.raw_os_error() != Some(EPIPE) {
                    perror("write");
                }
                if status.is_success() {
                    status = ProcStatus::from_exit_code(1);
                }
            }
        }
        f.internal_proc.mark_exited(status);
    });
}

/// If `outdata` or `errdata` are both empty, then mark the process as completed immediately.
/// Otherwise, run an internal process.
fn run_internal_process_or_short_circuit(
    parser: &Parser,
    j: &Job,
    p: &Process,
    outdata: Vec<u8>,
    errdata: Vec<u8>,
    ios: &IoChain,
) {
    if outdata.is_empty() && errdata.is_empty() {
        p.completed.store(true);
        if p.is_last_in_job {
            FLOGF!(
                exec_job_status,
                "Set status of job %d (%s) to %d using short circuit",
                j.job_id(),
                j.preview(),
                p.status().status_value()
            );
            if let Some(statuses) = j.get_statuses() {
                parser.set_last_statuses(statuses);
                parser.libdata_mut().status_count += 1;
            } else if j.flags().negate {
                // Special handling for `not set var (substitution)`.
                // If there is no status, but negation was requested,
                // take the last status and negate it.
                let mut last_statuses = parser.get_last_statuses();
                last_statuses.status = if last_statuses.status == 0 { 1 } else { 0 };
                parser.set_last_statuses(last_statuses);
            }
        }
    } else {
        run_internal_process(p, outdata, errdata, ios);
    }
}

/// Different ways to assign a pgroup for a process.
#[derive(Copy, Clone)]
pub enum PgroupPolicy {
    Inherit,           // Inherit fish's pgroup.
    Join(libc::pid_t), // Join a specific pgroup.
    Lead,              // The new process is the leader of a new pgroup.
}

/// Call fork() as part of executing a process in a job. Set up process groups.
/// Execute `child_action` in the context of the child.
fn fork_child_for_process(
    job: &Job,
    p: &Process,
    dup2s: &Dup2List,
    pgroup_policy: PgroupPolicy,
    child_action: impl FnOnce(&Process),
) -> LaunchResult {
    // Claim the tty from fish, if the job wants it and we are the pgroup leader.
    let claim_tty_from = if p.leads_pgrp && job.group().wants_terminal() {
        // getpgrp(2) cannot fail and always returns the (positive) caller's pgid
        Some(NonZeroU32::new(crate::nix::getpgrp() as u32).unwrap())
    } else {
        None
    };

    // Decide if the job wants to set a custom sigmask.
    let mut blocked_signals = MaybeUninit::uninit();
    let mut blocked_signals = unsafe {
        libc::sigemptyset(blocked_signals.as_mut_ptr());
        blocked_signals.assume_init()
    };
    let blocked_signals = if blocked_signals_for_job(job, &mut blocked_signals) {
        Some(&blocked_signals)
    } else {
        None
    };

    // Narrow the command name for error reporting before fork,
    // to avoid allocations in the forked child.
    let narrow_cmd = wcs2zstring(job.command());
    let narrow_argv0 = wcs2zstring(p.argv0().unwrap_or_default());
    let job_id = job.job_id().as_num();

    // Time to fork.
    let fork_res = execute_fork();
    if fork_res < 0 {
        return Err(());
    }

    // Determine the child pid.
    let is_parent = fork_res > 0;
    let pid: libc::pid_t = if is_parent { fork_res } else { getpid() };

    // Send the process to a new pgroup if requested.
    // Do this in BOTH the parent and child, to resolve the well-known race.
    if let Some(pgid) = match pgroup_policy {
        PgroupPolicy::Inherit => None,
        PgroupPolicy::Join(pgid) => Some(pgid),
        PgroupPolicy::Lead => Some(pid),
    } {
        let err = execute_setpgid(pid, pgid, is_parent);
        // Note this error is not fatal.
        if err != 0 {
            report_setpgid_error(
                err,
                is_parent,
                pid,
                pgid,
                job_id,
                &narrow_cmd,
                &narrow_argv0,
            )
        }
    }

    if !is_parent {
        // Set up the child process and run its action.
        child_setup_process(claim_tty_from, blocked_signals, true, dup2s);
        child_action(p);
        panic!("Child process returned control to fork_child!");
    }

    // We are the parent. Record the pid and store the pgid for the job if it should lead the pgroup.
    let pid = Pid::new(pid);
    p.set_pid(pid);
    if matches!(pgroup_policy, PgroupPolicy::Lead) {
        job.group().set_pgid(pid);
    }

    let count = FORK_COUNT.fetch_add(1, Ordering::Relaxed) + 1;
    FLOGF!(
        exec_fork,
        "Fork #%d, pid %d fork external command for '%s'",
        count,
        pid,
        p.argv0().unwrap()
    );
    Ok(())
}

/// Return an newly allocated output stream for the given fd, which is typically stdout or stderr.
/// This inspects the io_chain and decides what sort of output stream to return.
/// If `piped_output_needs_buffering` is set, and if the output is going to a pipe, then the other
/// end then synchronously writing to the pipe risks deadlock, so we must buffer it.
fn create_output_stream_for_builtin(
    fd: RawFd,
    io_chain: &IoChain,
    piped_output_needs_buffering: bool,
) -> OutputStream {
    let Some(io) = io_chain.io_for_fd(fd) else {
        // Common case of no redirections.
        // Just write to the fd directly.
        return OutputStream::Fd(FdOutputStream::new(fd));
    };
    match io.io_mode() {
        IoMode::bufferfill => {
            // Our IO redirection is to an internal buffer, e.g. a command substitution.
            // We will write directly to it.
            let buffer = io.as_bufferfill().unwrap().buffer();
            OutputStream::Buffered(BufferedOutputStream::new(buffer.clone()))
        }
        IoMode::close => {
            // Like 'echo foo >&-'
            OutputStream::Null
        }
        IoMode::file => {
            // Output is to a file which has been opened.
            OutputStream::Fd(FdOutputStream::new(io.source_fd()))
        }
        IoMode::pipe => {
            // Output is to a pipe. We may need to buffer.
            if piped_output_needs_buffering {
                OutputStream::String(StringOutputStream::new())
            } else {
                OutputStream::Fd(FdOutputStream::new(io.source_fd()))
            }
        }
        IoMode::fd => {
            // This is a case like 'echo foo >&5'
            // It's uncommon and unclear what should happen.
            OutputStream::String(StringOutputStream::new())
        }
    }
}

/// Handle output from a builtin, by printing the contents of builtin_io_streams to the redirections
/// given in io_chain.
fn handle_builtin_output(
    parser: &Parser,
    j: &Job,
    p: &Process,
    io_chain: &IoChain,
    out: &OutputStream,
    err: &OutputStream,
) {
    assert!(p.is_builtin(), "Process is not a builtin");

    // Figure out any data remaining to write. We may have none, in which case we can short-circuit.
    let outbuff = wcs2bytes(out.contents());
    let errbuff = wcs2bytes(err.contents());

    // Some historical behavior.
    if !outbuff.is_empty() {
        let _ = std::io::stdout().flush();
    }
    if !errbuff.is_empty() {
        let _ = std::io::stderr().flush();
    }

    // Construct and run our background process.
    run_internal_process_or_short_circuit(parser, j, p, outbuff, errbuff, io_chain);
}

/// Executes an external command.
/// An error return here indicates that the process failed to launch, and the rest of
/// the pipeline should be cancelled.
fn exec_external_command(
    parser: &Parser,
    j: &Job,
    p: &Process,
    proc_io_chain: &IoChain,
) -> LaunchResult {
    assert!(p.is_external(), "Process is not external");
    // Get argv and envv before we fork.
    let narrow_argv = p.argv().iter().map(|s| wcs2zstring(s)).collect();
    let argv = OwningNullTerminatedArray::new(narrow_argv);

    // Convert our IO chain to a dup2 sequence.
    let dup2s = dup2_list_resolve_chain(proc_io_chain);

    // Decide on pgroups - either we stay in fish's pgroup, or we set the pgroup from our leader,
    // or we become the leader.
    let pgroup_policy = if p.leads_pgrp {
        PgroupPolicy::Lead
    } else if let Some(pgid) = j.group().get_pgid() {
        PgroupPolicy::Join(pgid.as_pid_t())
    } else {
        PgroupPolicy::Inherit
    };

    // Ensure that stdin is blocking before we hand it off (see issue #176).
    // Note this will also affect stdout and stderr if they refer to the same tty.
    let _ = make_fd_blocking(STDIN_FILENO);

    let envv = parser.vars().export_array();

    let actual_cmd = wcs2zstring(&p.actual_cmd);

    #[cfg(FISH_USE_POSIX_SPAWN)]
    // Prefer to use posix_spawn, since it's faster on some systems like OS X.
    if can_use_posix_spawn_for_job(j, &dup2s) {
        let file = &parser.libdata().current_filename;
        let count = FORK_COUNT.fetch_add(1, Ordering::Relaxed) + 1; // spawn counts as a fork+exec

        let pid = PosixSpawner::new(j, pgroup_policy, &dup2s).and_then(|mut spawner| {
            spawner.spawn(actual_cmd.as_ptr(), argv.get_mut(), envv.get_mut())
        });
        let pid = match pid {
            Ok(pid) => pid,
            Err(err) => {
                safe_report_exec_error(err.0, &actual_cmd, &argv, &envv);
                p.status
                    .set(ProcStatus::from_exit_code(exit_code_from_exec_error(err.0)));
                return Err(());
            }
        };
        FLOGF!(
            exec_fork,
            "Fork #%d, pid %d: spawn external command '%s' from '%s'",
            count,
            pid,
            p.actual_cmd,
            file.as_ref()
                .map(|s| s.as_utfstr())
                .unwrap_or(L!("<no file>"))
        );

        // these are all things do_fork() takes care of normally (for forked processes):
        let pid = Pid::new(pid);
        p.set_pid(pid);
        if p.leads_pgrp {
            j.group().set_pgid(pid);
            // posix_spawn should in principle set the pgid before returning.
            // In glibc, posix_spawn uses fork() and the pgid group is set on the child side;
            // therefore the parent may not have seen it be set yet.
            // Ensure it gets set. See #4715, also https://github.com/Microsoft/WSL/issues/2997.
            execute_setpgid(pid.as_pid_t(), pid.as_pid_t(), true /* is parent */);
        }
        return Ok(());
    }

    fork_child_for_process(j, p, &dup2s, pgroup_policy, |p| {
        safe_launch_process(p, &actual_cmd, &argv, &envv)
    })
}

// Given that we are about to execute a function, push a function block and set up the
// variable environment.
fn function_prepare_environment(
    parser: &Parser,
    mut argv: Vec<WString>,
    props: &FunctionProperties,
) -> BlockId {
    // Extract the function name and remaining arguments.
    let mut func_name = WString::new();
    if !argv.is_empty() {
        // Extract and remove the function name from argv.
        func_name = argv.remove(0);
    }

    let fb = parser.push_block(Block::function_block(
        func_name,
        argv.clone(),
        props.shadow_scope,
    ));
    let vars = parser.vars();

    // Setup the environment for the function. There are three components of the environment:
    // 1. named arguments
    // 2. inherited variables
    // 3. argv

    let mut overwrite_argv = false;
    for (idx, named_arg) in props.named_arguments.iter().enumerate() {
        if named_arg == L!("argv") {
            overwrite_argv = true
        };
        if idx < argv.len() {
            vars.set_one(named_arg, EnvMode::LOCAL | EnvMode::USER, argv[idx].clone());
        } else {
            vars.set_empty(named_arg, EnvMode::LOCAL | EnvMode::USER);
        }
    }

    for (key, value) in &*props.inherit_vars {
        if key == L!("argv") {
            overwrite_argv = true
        };
        vars.set(key, EnvMode::LOCAL | EnvMode::USER, value.clone());
    }

    if !overwrite_argv {
        vars.set_argv(argv);
    }
    fb
}

// Given that we are done executing a function, restore the environment.
fn function_restore_environment(parser: &Parser, block: BlockId) {
    parser.pop_block(block);

    // If we returned due to a return statement, then stop returning now.
    parser.libdata_mut().returning = false;
}

// The "performer" function of a block or function process.
// This accepts a place to execute as `parser` and then executes the result, returning a status.
// This is factored out in this funny way in preparation for concurrent execution.
type ProcPerformer =
    dyn FnOnce(&Parser, Option<&mut OutputStream>, Option<&mut OutputStream>) -> ProcStatus;

// Return a function which may be to run the given block node process 'p'.
fn get_performer_for_block_node(p: &Process, job: &Job, io_chain: &IoChain) -> Box<ProcPerformer> {
    let ProcessType::BlockNode(node) = &p.typ else {
        panic!("Expected a block node process");
    };

    // We want to capture the job group.
    let job_group = job.group.clone();
    let io_chain = io_chain.clone();
    let node = node.clone();
    Box::new(move |parser: &Parser, _out, _err| {
        parser
            .eval_node(&node, &io_chain, job_group.as_ref(), BlockType::top, false)
            .status
    })
}

// Return a function which may be to run the given process 'p'.
// May return None in the rare case that the to-be called fish function no longer
// exists. This is just an artifact of the fact that we only capture the functions name, not its
// properties, when creating the job; thus a race could delete the function before we fetch its
// properties. Note this is user visible. An example:
//
//    function foo; echo hi; end
//    foo (functions --erase foo)
//
fn get_performer_for_function(
    p: &Process,
    job: &Job,
    io_chain: &IoChain,
) -> Result<Box<ProcPerformer>, ()> {
    assert!(p.is_function());
    // We want to capture the job group.
    let job_group = job.group.clone();
    let io_chain = io_chain.clone();
    // This may occur if the function was erased as part of its arguments or in other strange edge cases.
    let Some(props) = function::get_props(p.argv0().unwrap()) else {
        FLOG!(
            error,
            wgettext_fmt!("Unknown function '%s'", p.argv0().unwrap())
        );
        return Err(());
    };
    let argv = p.argv().clone();
    Ok(Box::new(move |parser: &Parser, _out, _err| {
        // Pull out the job list from the function.
        let fb = function_prepare_environment(parser, argv, &props);
        let body_node = props.func_node.child_ref(|n| &n.jobs);
        let mut res = parser.eval_node(
            &body_node,
            &io_chain,
            job_group.as_ref(),
            BlockType::top,
            false,
        );
        function_restore_environment(parser, fb);

        // If the function did not execute anything, treat it as success.
        if res.was_empty {
            res = EvalRes::new(ProcStatus::from_exit_code(EXIT_SUCCESS));
        }
        res.status
    }))
}

/// Execute a block node or function "process".
/// `piped_output_needs_buffering` if true, buffer the output.
fn exec_block_or_func_process(
    parser: &Parser,
    j: &Job,
    p: &Process,
    mut io_chain: IoChain,
    piped_output_needs_buffering: bool,
) -> LaunchResult {
    assert!(p.is_block_node() || p.is_function());
    // Create an output buffer if we're piping to another process.
    let mut block_output_bufferfill = None;
    if piped_output_needs_buffering {
        // Be careful to handle failure, e.g. too many open fds.
        match IoBufferfill::create() {
            Ok(tmp) => {
                // Teach the job about its bufferfill, and add it to our io chain.
                io_chain.push(tmp.clone());
                block_output_bufferfill = Some(tmp);
            }
            Err(_) => return Err(()),
        }
    }

    let performer = if p.is_block_node() {
        get_performer_for_block_node(p, j, &io_chain)
    } else {
        // Note this may fail if the function was erased.
        get_performer_for_function(p, j, &io_chain)?
    };
    p.status.set(performer(parser, None, None));

    // If we have a block output buffer, populate it now.
    let mut buffer_contents = vec![];
    if let Some(block_output_bufferfill) = block_output_bufferfill {
        // Remove our write pipe and forget it. This may close the pipe, unless another thread has
        // claimed it (background write) or another process has inherited it.
        io_chain.remove(&*block_output_bufferfill);
        buffer_contents = IoBufferfill::finish(block_output_bufferfill).newline_serialized();
    }

    run_internal_process_or_short_circuit(
        parser,
        j,
        p,
        buffer_contents,
        vec![], /* errdata */
        &io_chain,
    );

    Ok(())
}

fn get_performer_for_builtin(p: &Process, j: &Job, io_chain: &IoChain) -> Box<ProcPerformer> {
    assert!(p.is_builtin(), "Process must be a builtin");

    // Determine if we have a "direct" redirection for stdin.
    let mut stdin_is_directly_redirected = false;
    if !p.is_first_in_job {
        // We must have a pipe
        stdin_is_directly_redirected = true;
    } else {
        // We are not a pipe. Check if there is a redirection local to the process
        // that's not io_mode_t::close.
        for redir in p.redirection_specs() {
            if redir.fd == STDIN_FILENO && !redir.is_close() {
                stdin_is_directly_redirected = true;
                break;
            }
        }
    }

    // Pull out some fields which we want to copy. We don't want to store the process or job in the
    // returned closure.
    let job_group = j.group.clone();
    let io_chain = io_chain.clone();

    // Be careful to not capture p or j by value, as the intent is that this may be run on another
    // thread.
    let argv = p.argv().clone();
    Box::new(
        move |parser: &Parser,
              output_stream: Option<&mut OutputStream>,
              errput_stream: Option<&mut OutputStream>| {
            let output_stream = output_stream.unwrap();
            let errput_stream = errput_stream.unwrap();
            let out_io = io_chain.io_for_fd(STDOUT_FILENO);
            let err_io = io_chain.io_for_fd(STDERR_FILENO);

            // Figure out what fd to use for the builtin's stdin.
            let mut local_builtin_stdin = STDIN_FILENO;
            if let Some(inp) = io_chain.io_for_fd(STDIN_FILENO) {
                // Ignore fd redirections from an fd other than the
                // standard ones. e.g. in source <&3 don't actually read from fd 3,
                // which is internal to fish. We still respect this redirection in
                // that we pass it on as a block IO to the code that source runs,
                // and therefore this is not an error.
                let ignore_redirect = inp.io_mode() == IoMode::fd && inp.source_fd() >= 3;
                if !ignore_redirect {
                    local_builtin_stdin = inp.source_fd();
                }
            }

            // Populate our IoStreams. This is a bag of information for the builtin.
            let mut streams = IoStreams::new(output_stream, errput_stream, &io_chain);
            streams.job_group = job_group;
            streams.stdin_fd = local_builtin_stdin;
            streams.stdin_is_directly_redirected = stdin_is_directly_redirected;
            streams.out_is_redirected = out_io.is_some();
            streams.err_is_redirected = err_io.is_some();
            streams.out_is_piped = out_io.is_some_and(|io| io.io_mode() == IoMode::pipe);
            streams.err_is_piped = err_io.is_some_and(|io| io.io_mode() == IoMode::pipe);

            // Disallow nul bytes in the arguments, as they are not allowed in builtins.
            let mut shim_argv: Vec<&wstr> =
                argv.iter().map(|s| truncate_at_nul(s.as_ref())).collect();
            // Execute the builtin.
            builtin_run(parser, &mut shim_argv, &mut streams)
        },
    )
}

/// Executes a builtin "process".
fn exec_builtin_process(
    parser: &Parser,
    j: &Job,
    p: &Process,
    io_chain: &IoChain,
    piped_output_needs_buffering: bool,
) -> LaunchResult {
    assert!(p.is_builtin(), "Process is not a builtin");
    let mut out =
        create_output_stream_for_builtin(STDOUT_FILENO, io_chain, piped_output_needs_buffering);
    let mut err =
        create_output_stream_for_builtin(STDERR_FILENO, io_chain, piped_output_needs_buffering);

    let performer = get_performer_for_builtin(p, j, io_chain);
    let status = performer(parser, Some(&mut out), Some(&mut err));
    p.status.set(status);
    handle_builtin_output(parser, j, p, io_chain, &out, &err);
    Ok(())
}

#[derive(Default)]
struct PartialPipes {
    /// Read end of the pipe.
    read: Option<OwnedFd>,
    /// Write end of the pipe.
    write: Option<OwnedFd>,
}

/// Executes a process \p `in` `job`, using the pipes `pipes` (which may have invalid fds if this
/// is the first or last process).
/// `deferred_pipes` represents the pipes from our deferred process; if set ensure they get closed
/// in any child. If `is_deferred_run` is true, then this is a deferred run; this affects how
/// certain buffering works.
/// An error return here indicates that the process failed to launch, and the rest of
/// the pipeline should be cancelled.
fn exec_process_in_job(
    parser: &Parser,
    p: &Process,
    j: &Job,
    block_io: IoChain,
    pipes: PartialPipes,
    deferred_pipes: &PartialPipes,
    is_deferred_run: bool,
) -> LaunchResult {
    // The write pipe (destined for stdout) needs to occur before redirections. For example,
    // with a redirection like this:
    //
    //   `foo 2>&1 | bar`
    //
    // what we want to happen is this:
    //
    //    dup2(pipe, stdout)
    //    dup2(stdout, stderr)
    //
    // so that stdout and stderr both wind up referencing the pipe.
    //
    // The read pipe (destined for stdin) is more ambiguous. Imagine a pipeline like this:
    //
    //   echo alpha | cat < beta.txt
    //
    // Should cat output alpha or beta? bash and ksh output 'beta', tcsh gets it right and
    // complains about ambiguity, and zsh outputs both (!). No shells appear to output 'alpha',
    // so we match bash here. That would mean putting the pipe first, so that it gets trumped by
    // the file redirection.
    //
    // However, eval does this:
    //
    //   echo "begin; $argv "\n" ;end <&3 3<&-" | source 3<&0
    //
    // which depends on the redirection being evaluated before the pipe. So the write end of the
    // pipe comes first, the read pipe of the pipe comes last. See issue #966.

    // Maybe trace this process.
    // TODO: 'and' and 'or' will not show.
    trace_if_enabled_with_args(parser, L!(""), p.argv());

    // The IO chain for this process.
    let mut process_net_io_chain = block_io;

    if let Some(fd) = pipes.write {
        process_net_io_chain.push(Arc::new(IoPipe::new(
            p.pipe_write_fd,
            false, /* not input */
            fd,
        )));
    }

    // Append IOs from the process's redirection specs.
    // This may fail, e.g. a failed redirection.
    if !process_net_io_chain
        .append_from_specs(p.redirection_specs(), &parser.vars().get_pwd_slash())
    {
        return Err(());
    }

    // Read pipe goes last.
    if let Some(fd) = pipes.read {
        let pipe_read = Arc::new(IoPipe::new(STDIN_FILENO, true /* input */, fd));
        process_net_io_chain.push(pipe_read);
    }

    // If we have stashed pipes, make sure those get closed in the child.
    for afd in [&deferred_pipes.read, &deferred_pipes.write]
        .into_iter()
        .flatten()
    {
        process_net_io_chain.push(Arc::new(IoClose::new(afd.as_raw_fd())));
    }

    if !p.is_block_node() {
        // A simple `begin ... end` should not be considered an execution of a command.
        parser.libdata_mut().exec_count += 1;
    }

    let mut block_id = None;
    if !p.variable_assignments.is_empty() {
        block_id = Some(parser.push_block(Block::variable_assignment_block()));
    }
    let _pop_block = ScopeGuard::new((), |()| {
        if let Some(block_id) = block_id {
            parser.pop_block(block_id);
        }
    });
    for assignment in &p.variable_assignments {
        parser.vars().set(
            &assignment.variable_name,
            EnvMode::LOCAL | EnvMode::EXPORT,
            assignment.values.clone(),
        );
    }

    // Decide if outputting to a pipe may deadlock.
    // This happens if fish pipes from an internal process into another internal process:
    //    echo $big | string match...
    // Here fish will only run one process at a time, so the pipe buffer may overfill.
    // It may also happen when piping internal -> external:
    //    echo $big | external_proc
    // fish wants to run `echo` before launching external_proc, so the pipe may deadlock.
    // However if we are a deferred run, it means that we are piping into an external process
    // which got launched before us!
    let piped_output_needs_buffering = !p.is_last_in_job && !is_deferred_run;

    // Execute the process.
    p.check_generations_before_launch();
    match p.typ {
        ProcessType::Function | ProcessType::BlockNode(_) => exec_block_or_func_process(
            parser,
            j,
            p,
            process_net_io_chain,
            piped_output_needs_buffering,
        ),
        ProcessType::Builtin => exec_builtin_process(
            parser,
            j,
            p,
            &process_net_io_chain,
            piped_output_needs_buffering,
        ),
        ProcessType::External => {
            parser.libdata_mut().exec_external_count += 1;
            exec_external_command(parser, j, p, &process_net_io_chain)?;
            // It's possible (though unlikely) that this is a background process which recycled a
            // pid from another, previous background process. Forget any such old process.
            parser.mut_wait_handles().remove_by_pid(p.pid().unwrap());
            Ok(())
        }
        ProcessType::Exec => {
            // We should have handled exec up above.
            panic!(
                "process_type_t::exec process found in pipeline, where it should never be. Aborting."
            );
        }
    }
}

// Do we have a fish internal process that pipes into a real process? If so, we are going to
// launch it last (if there's more than one, just the last one). That is to prevent buffering
// from blocking further processes. See #1396.
// Example:
//   for i in (seq 1 5); sleep 1; echo $i; end | cat
// This should show the output as it comes, not buffer until the end.
// Any such process (only one per job) will be called the "deferred" process.
fn get_deferred_process(j: &Job) -> Option<usize> {
    // Common case is no deferred proc.
    if j.processes().len() <= 1 {
        return None;
    }

    // Skip execs, which can only appear at the front.
    if matches!(j.processes()[0].typ, ProcessType::Exec) {
        return None;
    }

    // Find the last non-external process, and return it if it pipes into an external process.
    for (i, p) in j.processes().iter().enumerate().rev() {
        if !p.is_external() {
            return if p.is_last_in_job { None } else { Some(i) };
        }
    }
    None
}

/// Given that we failed to execute process `failed_proc` in job `job`, mark that process and
/// every subsequent process in the pipeline as aborted before launch.
fn abort_pipeline_from(job: &Job, offset: usize) {
    for p in job.processes().iter().skip(offset) {
        p.mark_aborted_before_launch();
    }
}

// Given that we are about to execute an exec() call, check if the parser is interactive and there
// are extant background jobs. If so, warn the user and do not exec().
// Return true if we should allow exec, false to disallow it.
fn allow_exec_with_background_jobs(parser: &Parser) -> bool {
    // If we're not interactive, we cannot warn.
    if !parser.is_interactive() {
        return true;
    }

    // Construct the list of running background jobs.
    let bgs = jobs_requiring_warning_on_exit(parser);
    if bgs.is_empty() {
        return true;
    }

    // Compare run counts, so we only warn once.
    let current_run_count = reader_run_count();
    let last_exec_run_count = &mut parser.libdata_mut().last_exec_run_counter;
    if isatty(STDIN_FILENO) && current_run_count - 1 != *last_exec_run_count {
        print_exit_warning_for_jobs(&bgs);
        *last_exec_run_count = current_run_count;
        false
    } else {
        hup_jobs(&parser.jobs());
        true
    }
}

/// Populate `lst` with the output of `buffer`, perhaps splitting lines according to `split`.
fn populate_subshell_output(lst: &mut Vec<WString>, buffer: &SeparatedBuffer, split: bool) {
    // Walk over all the elements.
    for elem in buffer.elements() {
        let data = &elem.contents;
        if elem.is_explicitly_separated() {
            // Just append this one.
            lst.push(bytes2wcstring(data));
            continue;
        }

        // Not explicitly separated. We have to split it explicitly.
        assert!(
            !elem.is_explicitly_separated(),
            "should not be explicitly separated"
        );
        if split {
            let mut cursor = 0;
            while cursor < data.len() {
                // Look for the next separator.
                let stop = data[cursor..].iter().position(|c| *c == b'\n');
                let hit_separator = stop.is_some();
                // If it's not found, just use the end.
                let stop = stop.map(|rel| cursor + rel).unwrap_or(data.len());
                // Stop now points at the first character we do not want to copy.
                lst.push(bytes2wcstring(&data[cursor..stop]));

                // If we hit a separator, skip over it; otherwise we're at the end.
                cursor = stop + if hit_separator { 1 } else { 0 };
            }
        } else {
            // We're not splitting output, but we still want to trim off a trailing newline.
            let trailing_newline = if data.last() == Some(&b'\n') { 1 } else { 0 };
            lst.push(bytes2wcstring(&data[..data.len() - trailing_newline]));
        }
    }
}

/// Execute `cmd` in a subshell in `parser`. If `lst` is not null, populate it with the output.
/// Return $status in `out_status`.
/// If `job_group` is set, any spawned commands should join that job group.
/// If `apply_exit_status` is false, then reset $status back to its original value.
/// `is_subcmd` controls whether we apply a read limit.
/// `break_expand` is used to propagate whether the result should be "expansion breaking" in the
/// sense that subshells used during string expansion should halt that expansion. Return the value
/// of $status.
fn exec_subshell_internal(
    cmd: &wstr,
    parser: &Parser,
    job_group: Option<&JobGroupRef>,
    lst: Option<&mut Vec<WString>>,
    break_expand: &mut bool,
    apply_exit_status: bool,
    is_subcmd: bool,
) -> Result<(), ErrorCode> {
    let _scoped = parser.push_scope(|s| {
        s.is_subshell = true;
        s.read_limit = if is_subcmd {
            READ_BYTE_LIMIT.load(Ordering::Relaxed)
        } else {
            0
        };
    });

    let prev_statuses = parser.get_last_statuses();
    let _put_back = ScopeGuard::new((), |()| {
        if !apply_exit_status {
            parser.set_last_statuses(prev_statuses);
        }
    });

    let split_output = parser.vars().get_unless_empty(L!("IFS")).is_some();

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may
    // be null.
    let Ok(bufferfill) = IoBufferfill::create_opts(parser.scope().read_limit, STDOUT_FILENO) else {
        *break_expand = true;
        return Err(STATUS_CMD_ERROR);
    };

    let mut io_chain = IoChain::new();
    io_chain.push(bufferfill.clone());
    let eval_res = parser.eval_with(cmd, &io_chain, job_group, BlockType::subst, false);
    let buffer = IoBufferfill::finish(bufferfill);
    if buffer.discarded() {
        *break_expand = true;
        return Err(STATUS_READ_TOO_MUCH);
    }

    if eval_res.break_expand {
        *break_expand = true;
        match eval_res.status.status_value() {
            0 => return Ok(()),
            code => return Err(code),
        }
    }

    if let Some(lst) = lst {
        populate_subshell_output(lst, &buffer, split_output);
    }
    *break_expand = false;
    match eval_res.status.status_value() {
        0 => Ok(()),
        code => Err(code),
    }
}
