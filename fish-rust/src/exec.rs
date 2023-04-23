// Functions for executing a program.
//
// Some of the code in this file is based on code from the Glibc manual, though the changes
// performed have been massive.

use crate::builtins::shared::{
    builtin_run, STATUS_CMD_ERROR, STATUS_CMD_OK, STATUS_CMD_UNKNOWN, STATUS_NOT_EXECUTABLE,
    STATUS_READ_TOO_MUCH,
};
use crate::common::{
    exit_without_destructors, scoped_push, str2wcstring, wcs2string, wcs2zstring, write_loop,
    Cleanup,
};
use crate::compat::_PATH_BSHELL;
use crate::env::{EnvMode, EnvStack, Environment, Statuses};
use crate::env_dispatch::{get_use_posix_spawn, READ_BYTE_LIMIT};
use crate::fds::{make_autoclose_pipes, open_cloexec, AutoCloseFd, AutoClosePipes, PIPE_ERROR};
use crate::flog::FLOGF;
use crate::function::{function_get_props, FunctionProperties};
use crate::io::{
    BufferedOutputStream, FdOutputStream, IoBufferfill, IoChain, IoClose, IoMode, IoPipe,
    IoStreams, NullOutputStream, OutputStream, OutputStreamRef, SeparatedBuffer,
    StringOutputStream,
};
use crate::null_terminated_array::{
    null_terminated_array_length, AsNullTerminatedArray, OwningNullTerminatedArray,
};
use crate::parser::{Block, BlockId, BlockType, EvalRes, Parser};
use crate::postfork::{
    child_setup_process, execute_fork, execute_setpgid, report_setpgid_error,
    safe_report_exec_error,
};
use crate::proc::{
    hup_jobs, is_interactive_session, jobs_requiring_warning_on_exit, no_exec,
    print_exit_warning_for_jobs, InternalProc, Job, JobGroupRef, JobRef, ProcStatus, Process,
    ProcessType, TtyTransfer, INVALID_PID,
};
use crate::reader::{reader_run_count, restore_term_mode};
use crate::redirection::{dup2_list_resolve_chain, Dup2List};
use crate::threads::{iothread_perform_cant_wait, is_forked_child};
use crate::timer::push_timer;
use crate::trace::{trace_if_enabled, trace_if_enabled_with_args};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::ToWString;
use crate::wutil::{fish_wcstoi, make_fd_blocking, perror};
use crate::wutil::{wgettext, wgettext_fmt};
use errno::{errno, set_errno};
use libc::{
    c_char, isatty, EACCES, ENOENT, ENOEXEC, ENOTDIR, EPIPE, EXIT_FAILURE, EXIT_SUCCESS, O_NOCTTY,
    O_RDONLY, SIGINT, SIGQUIT, STDERR_FILENO, STDIN_FILENO, STDOUT_FILENO,
};
use std::ffi::CStr;
use std::io::{Read, Write};
use std::os::fd::{FromRawFd, RawFd};
use std::rc::Rc;
use std::slice;
use std::sync::atomic::Ordering;
use std::sync::{atomic::AtomicUsize, Arc, RwLock};
use widestring_suffix::widestrs;

/// Execute the processes specified by \p j in the parser \p.
/// On a true return, the job was successfully launched and the parser will take responsibility for
/// cleaning it up. On a false return, the job could not be launched and the caller must clean it
/// up.
pub fn exec_job(parser: &mut Parser, job: &JobRef, block_io: &IoChain) -> bool {
    let mut j = job.write().unwrap();
    // If fish was invoked with -n or --no-execute, then no_exec will be set and we do nothing.
    if no_exec() {
        return true;
    }

    // Handle an exec call.
    if j.processes[0].typ == ProcessType::exec {
        // If we are interactive, perhaps disallow exec if there are background jobs.
        if !allow_exec_with_background_jobs(parser) {
            for p in &mut j.processes {
                p.mark_aborted_before_launch();
            }
            return false;
        }

        internal_exec(parser.vars(), &mut j, block_io);
        // internal_exec only returns if it failed to set up redirections.
        // In case of an successful exec, this code is not reached.
        let status = if j.flags().negate { 0 } else { 1 };
        parser.set_last_statuses(Statuses::just(status));

        // A false return tells the caller to remove the job from the list.
        for p in &mut j.processes {
            p.mark_aborted_before_launch();
        }
        return false;
    }
    let timer = push_timer(j.wants_timing() && !no_exec());

    // Get the deferred process, if any. We will have to remember its pipes.
    let mut deferred_pipes = AutoClosePipes::default();
    let deferred_process = get_deferred_process(&j);

    // We may want to transfer tty ownership to the pgroup leader.
    let mut transfer = TtyTransfer::new();

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
    let mut pipe_next_read = AutoCloseFd::empty();
    let mut aborted_pipeline = false;
    let mut procs_launched = 0;
    for i in 0..j.processes.len() {
        let p = &j.processes[i];
        // proc_pipes is the pipes applied to this process. That is, it is the read end
        // containing the output of the previous process (if any), plus the write end that will
        // output to the next process (if any).
        let mut proc_pipes = AutoClosePipes::default();
        std::mem::swap(&mut proc_pipes.read, &mut pipe_next_read);
        if !p.is_last_in_job {
            let Some(pipes) = make_autoclose_pipes() else {
                    FLOGF!(warning, "%ls", &wgettext!(PIPE_ERROR));
                    perror("pipe");
                    aborted_pipeline = true;
                    abort_pipeline_from(&mut j, i);
                    break;
                };
            pipe_next_read = pipes.read;
            proc_pipes.write = pipes.write;

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
        let p = &mut j.processes[i];
        if exec_process_in_job(parser, p, job, block_io, proc_pipes, &deferred_pipes, false)
            .is_err()
        {
            aborted_pipeline = true;
            abort_pipeline_from(&mut j, i);
            break;
        }
        procs_launched += 1;

        // Transfer tty?
        if p.leads_pgrp && j.group().wants_terminal() {
            transfer.to_job_group(j.group.as_ref().unwrap());
        }
    }
    pipe_next_read.close();

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
                &mut j.processes[dp],
                job,
                block_io,
                deferred_pipes,
                &AutoClosePipes::default(),
                true,
            )
            .is_err()
        {
            j.processes[dp].mark_aborted_before_launch();
        }
    }

    FLOGF!(
        exec_job_exec,
        "Executed job %d from command '%ls'",
        j.job_id(),
        j.command()
    );

    j.mark_constructed();

    // If exec_error then a backgrounded job would have been terminated before it was ever assigned
    // a pgroup, so error out before setting last_pid.
    if !j.is_foreground() {
        if let Some(last_pid) = j.get_last_pid() {
            parser
                .vars()
                .set_one(L!("last_pid"), EnvMode::GLOBAL, last_pid.to_wstring());
        } else {
            parser.vars().set_empty(L!("last_pid"), EnvMode::GLOBAL);
        }
    }

    if !j.is_initially_background() {
        j.continue_job(parser);
    }

    if j.is_stopped() {
        transfer.save_tty_modes();
    }
    transfer.reclaim();
    true
}

/// Evaluate a command.
///
/// \param cmd the command to execute
/// \param parser the parser with which to execute code
/// \param outputs if set, the list to insert output into.
/// \param apply_exit_status if set, update $status within the parser, otherwise do not.
///
/// \return a value appropriate for populating $status.
pub fn exec_subshell_(
    cmd: &wstr,
    parser: &mut Parser,
    outputs: Option<&mut Vec<WString>>,
    apply_exit_status: bool,
) -> libc::c_int {
    let mut break_expand = false;
    exec_subshell_internal(
        cmd,
        parser,
        &None,
        outputs,
        &mut break_expand,
        apply_exit_status,
        false,
    )
}

/// Like exec_subshell, but only returns expansion-breaking errors. That is, a zero return means
/// "success" (even though the command may have failed), a non-zero return means that we should
/// halt expansion. If the \p pgid is supplied, then any spawned external commands should join that
/// pgroup.
pub fn exec_subshell_for_expand(
    cmd: &wstr,
    parser: &mut Parser,
    job_group: &Option<JobGroupRef>,
    outputs: &mut Vec<WString>,
) -> libc::c_int {
    parser.assert_can_execute();
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
    if break_expand {
        ret
    } else {
        STATUS_CMD_OK.unwrap()
    }
}

/// Add signals that should be masked for external processes in this job.
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

/// Number of calls to fork() or posix_spawn().
static FORK_COUNT: AtomicUsize = AtomicUsize::new(0);

/// A launch_result_t indicates when a process failed to launch, and therefore the rest of the
/// pipeline should be aborted. This includes failed redirections, fd exhaustion, fork() failures,
/// etc.
type LaunchResult = Result<(), ()>;

/// Given an error \p err returned from either posix_spawn or exec, \return a process exit code.
fn exit_code_from_exec_error(err: libc::c_int) -> libc::c_int {
    assert!(err != 0, "Zero is success, not an error");
    match err {
        ENOENT | ENOTDIR => {
            // This indicates either the command was not found, or a file redirection was not found.
            // We do not use posix_spawn file redirections so this is always command-not-found.
            STATUS_CMD_UNKNOWN.unwrap()
        }
        EACCES | ENOEXEC => {
            // The file is not executable for various reasons.
            STATUS_NOT_EXECUTABLE.unwrap()
        }
        #[cfg(target_os = "macos")]
        libc::EBADARCH => {
            // This is for e.g. running ARM app on Intel Mac.
            STATUS_NOT_EXECUTABLE.unwrap()
        }
        _ => {
            // Generic failure.
            EXIT_FAILURE
        }
    }
}

/// This is a 'looks like text' check.
/// \return true if either there is no NUL byte, or there is a line containing a lowercase letter
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
    let fd = open_cloexec(path, O_RDONLY | O_NOCTTY, 0);
    if fd != -1 {
        let mut file = unsafe { std::fs::File::from_raw_fd(fd) };
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
    p: &mut Process,
    actual_cmd: &CStr,
    argv: &impl AsNullTerminatedArray<CharType = c_char>,
    envv: &impl AsNullTerminatedArray<CharType = c_char>,
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
        const maxargs: usize = 128;
        let nargs = null_terminated_array_length(argv.get());
        let argv = unsafe { slice::from_raw_parts(argv.get(), nargs) };
        if nargs <= maxargs {
            // +1 for /bin/sh, +1 for terminating nullptr
            let mut argv2 = [std::ptr::null(); 1 + maxargs + 1];
            let interp = *_PATH_BSHELL;
            argv2[0] = interp.as_ptr();
            argv2[1..].copy_from_slice(argv);
            // The command to call should use the full path,
            // not what we would pass as argv0.
            argv2[1] = actual_cmd.as_ptr();
            unsafe {
                libc::execve(_PATH_BSHELL.as_ptr(), &argv2[0], envv.get());
            }
        }
    }

    set_errno(err);
    safe_report_exec_error(errno(), actual_cmd, argv, envv);
    exit_without_destructors(exit_code_from_exec_error(err.0));
}

/// This function is similar to launch_process, except it is not called after a fork (i.e. it only
/// calls exec) and therefore it can allocate memory.
fn launch_process_nofork(vars: &EnvStack, p: &mut Process) -> ! {
    assert!(!is_forked_child());

    // Construct argv. Ensure the strings stay alive for the duration of this function.
    let narrow_strings = p.argv().iter().map(|s| wcs2zstring(s)).collect();
    let argv = OwningNullTerminatedArray::new(narrow_strings);

    // Construct envp.
    let envp = vars.export_arr();
    let actual_cmd = wcs2zstring(&p.actual_cmd);

    // Ensure the terminal modes are what they were before we changed them.
    restore_term_mode();
    // Bounce to launch_process. This never returns.
    safe_launch_process(p, &actual_cmd, &argv, &*envp);
}

// Returns whether we can use posix spawn for a given process in a given job.
//
// To avoid the race between the caller calling tcsetpgrp() and the client checking the
// foreground process group, we don't use posix_spawn if we're going to foreground the process. (If
// we use fork(), we can call tcsetpgrp after the fork, before the exec, and avoid the race).
fn can_use_posix_spawn_for_job(job: &JobRef, dup2s: &Dup2List) -> bool {
    // Is it globally disabled?
    if !get_use_posix_spawn() {
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
    let job = job.read().unwrap();
    let wants_terminal = job.group().wants_terminal();
    !wants_terminal
}

#[widestrs]
fn internal_exec(vars: &EnvStack, j: &mut Job, block_io: &IoChain) {
    // Do a regular launch -  but without forking first...
    let mut all_ios = block_io.clone();
    if !all_ios.append_from_specs(j.processes[0].redirection_specs(), &vars.get_pwd_slash()) {
        return;
    }

    // child_setup_process makes sure signals are properly set up.
    let redirs = dup2_list_resolve_chain(&all_ios);
    if child_setup_process(
        0, /* not claim_tty */
        j, false, /* not is_forked */
        &redirs,
    ) == 0
    {
        // Decrement SHLVL as we're removing ourselves from the shell "stack".
        if is_interactive_session() {
            let shlvl_var = vars.getf("SHLVL"L, EnvMode::GLOBAL | EnvMode::EXPORT);
            let mut shlvl_str = "0"L.to_owned();
            if let Some(shlvl_var) = shlvl_var {
                if let Ok(shlvl) = fish_wcstoi::<usize, _>(&shlvl_var.as_string()) {
                    if shlvl != 0 {
                        shlvl_str = (shlvl - 1).to_wstring();
                    }
                }
            }
            vars.set_one("SHLVL"L, EnvMode::GLOBAL | EnvMode::EXPORT, shlvl_str);
        }

        // launch_process _never_ returns.
        launch_process_nofork(vars, &mut *j.processes[0]);
    }
}

/// Construct an internal process for the process p. In the background, write the data \p outdata to
/// stdout and \p errdata to stderr, respecting the io chain \p ios. For example if target_fd is 1
/// (stdout), and there is a dup2 3->1, then we need to write to fd 3. Then exit the internal
/// process.
fn run_internal_process(p: &mut Process, outdata: Vec<u8>, errdata: Vec<u8>, ios: &IoChain) {
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
        internal_proc: Arc<RwLock<InternalProc>>,

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
    p.internal_proc = Some(Arc::new(RwLock::new(InternalProc::new())));
    let mut f = Box::new(WriteFields {
        src_outfd: -1,
        outdata,

        src_errfd: -1,
        errdata,

        ios: IoChain::default(),
        dup2s: Dup2List::new(),
        internal_proc: p.internal_proc.as_ref().unwrap().clone(),

        success_status: ProcStatus::default(),
    });

    FLOGF!(
        proc_internal_proc,
        "Created internal proc %llu to write output for proc '%ls'",
        f.internal_proc.read().unwrap().get_id(),
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
        f.internal_proc.write().unwrap().mark_exited(p.status);
        return;
    }

    // Ensure that ios stays alive, it may own fds.
    f.ios = ios.clone();

    // If our process is a builtin, it will have already set its status value. Make sure we
    // propagate that if our I/O succeeds and don't read it on a background thread. TODO: have
    // builtin_run provide this directly, rather than setting it in the process.
    f.success_status = p.status;

    iothread_perform_cant_wait(|| {
        let mut status = f.success_status;
        if !f.skip_out() {
            if let Err(err) = write_loop(&f.src_outfd, &f.outdata) {
                if err.raw_os_error().unwrap() != EPIPE {
                    perror("write");
                }
                if status.is_success() {
                    status = ProcStatus::from_exit_code(1);
                }
            }
        }
        if !f.skip_err() {
            if let Err(err) = write_loop(&f.src_errfd, &f.errdata) {
                if err.raw_os_error().unwrap() != EPIPE {
                    perror("write");
                }
                if status.is_success() {
                    status = ProcStatus::from_exit_code(1);
                }
            }
        }
        f.internal_proc.write().unwrap().mark_exited(status);
    });
}

/// If \p outdata or \p errdata are both empty, then mark the process as completed immediately.
/// Otherwise, run an internal process.
fn run_internal_process_or_short_circuit(
    parser: &mut Parser,
    j: &JobRef,
    p: &mut Process,
    outdata: Vec<u8>,
    errdata: Vec<u8>,
    ios: &IoChain,
) {
    if outdata.is_empty() && errdata.is_empty() {
        let j = j.read().unwrap();
        p.completed = true;
        if p.is_last_in_job {
            FLOGF!(
                exec_job_status,
                "Set status of job %d (%ls) to %d using short circuit",
                j.job_id(),
                j.preview(),
                p.status.status_value()
            );
            if let Some(statuses) = j.get_statuses() {
                parser.set_last_statuses(statuses);
                parser.libdata_mut().pods.status_count += 1;
            } else {
                // Special handling for `not set var (substitution)`.
                // If there is no status, but negation was requested,
                // take the last status and negate it.
                let mut last_statuses = parser.get_last_statuses();
                last_statuses.status = !last_statuses.status;
                parser.set_last_statuses(last_statuses);
            }
        }
    } else {
        run_internal_process(p, outdata, errdata, ios);
    }
}

/// Call fork() as part of executing a process \p p in a job \j. Execute \p child_action in the
/// context of the child.
fn fork_child_for_process(
    job: &JobRef,
    p: &mut Process,
    dup2s: &Dup2List,
    fork_type: &wstr,
    child_action: impl FnOnce(&mut Process),
) -> LaunchResult {
    // Claim the tty from fish, if the job wants it and we are the pgroup leader.
    let claim_tty_from = if p.leads_pgrp && job.read().unwrap().group().wants_terminal() {
        unsafe { libc::getpgrp() }
    } else {
        INVALID_PID
    };

    let pid = execute_fork();
    if pid < 0 {
        return Err(());
    }
    let is_parent = pid > 0;

    // Record the pgroup if this is the leader.
    // Both parent and child attempt to send the process to its new group, to resolve the race.
    p.pid = if is_parent {
        pid
    } else {
        unsafe { libc::getpid() }
    };
    if p.leads_pgrp {
        job.read().unwrap().group_mut().set_pgid(pid);
    }
    {
        if let Some(pgid) = job.read().unwrap().group().get_pgid() {
            let err = execute_setpgid(p.pid, pgid, is_parent);
            if err != 0 {
                report_setpgid_error(err, is_parent, pgid, &job.read().unwrap(), p);
            }
        }
    }

    if !is_parent {
        // Child process.
        child_setup_process(claim_tty_from, &job.read().unwrap(), true, dup2s);
        child_action(p);
        panic!("Child process returned control to fork_child lambda!");
    }

    let count = FORK_COUNT.fetch_add(1, Ordering::Relaxed) + 1;
    FLOGF!(
        exec_fork,
        "Fork #%d, pid %d: %s for '%ls'",
        count,
        pid,
        fork_type,
        p.argv0().unwrap()
    );
    Ok(())
}

/// \return an newly allocated output stream for the given fd, which is typically stdout or stderr.
/// This inspects the io_chain and decides what sort of output stream to return.
/// If \p piped_output_needs_buffering is set, and if the output is going to a pipe, then the other
/// end then synchronously writing to the pipe risks deadlock, so we must buffer it.
fn create_output_stream_for_builtin(
    fd: RawFd,
    io_chain: &IoChain,
    piped_output_needs_buffering: bool,
) -> Box<dyn OutputStream> {
    let Some(io) = io_chain.io_for_fd(fd) else {
        // Common case of no redirections.
        // Just write to the fd directly.
        return Box::new(FdOutputStream::new(fd));
    };
    match io.io_mode() {
        IoMode::bufferfill => {
            // Our IO redirection is to an internal buffer, e.g. a command substitution.
            // We will write directly to it.
            let buffer = io.as_bufferfill().unwrap().the_buffer();
            Box::new(BufferedOutputStream::new(buffer.clone()))
        }
        IoMode::close => {
            // Like 'echo foo >&-'
            Box::new(NullOutputStream::new())
        }
        IoMode::file => {
            // Output is to a file which has been opened.
            Box::new(FdOutputStream::new(io.source_fd()))
        }
        IoMode::pipe => {
            // Output is to a pipe. We may need to buffer.
            if piped_output_needs_buffering {
                Box::new(StringOutputStream::new())
            } else {
                Box::new(FdOutputStream::new(io.source_fd()))
            }
        }
        IoMode::fd => {
            // This is a case like 'echo foo >&5'
            // It's uncommon and unclear what should happen.
            Box::new(StringOutputStream::new())
        }
    }
}

/// Handle output from a builtin, by printing the contents of builtin_io_streams to the redirections
/// given in io_chain.

fn handle_builtin_output(
    parser: &mut Parser,
    j: &JobRef,
    p: &mut Process,
    io_chain: &IoChain,
    out: &dyn OutputStream,
    err: &dyn OutputStream,
) {
    assert!(p.typ == ProcessType::builtin, "Process is not a builtin");

    // Figure out any data remaining to write. We may have none, in which case we can short-circuit.
    let outbuff = wcs2string(out.contents());
    let errbuff = wcs2string(err.contents());

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
    parser: &mut Parser,
    j: &JobRef,
    p: &mut Process,
    proc_io_chain: &IoChain,
) -> LaunchResult {
    assert!(p.typ == ProcessType::external, "Process is not external");
    // Get argv and envv before we fork.
    let narrow_argv = p.argv().iter().map(|s| wcs2zstring(s)).collect();
    let argv = OwningNullTerminatedArray::new(narrow_argv);

    // Convert our IO chain to a dup2 sequence.
    let dup2s = dup2_list_resolve_chain(proc_io_chain);

    // Ensure that stdin is blocking before we hand it off (see issue #176).
    // Note this will also affect stdout and stderr if they refer to the same tty.
    make_fd_blocking(STDIN_FILENO);

    let envv = parser.vars().export_arr();

    let actual_cmd = wcs2zstring(&p.actual_cmd);
    let file = &parser.libdata().current_filename;

    #[cfg(FISH_USE_POSIX_SPAWN)]
    // Prefer to use posix_spawn, since it's faster on some systems like OS X.
    if can_use_posix_spawn_for_job(j, dup2s) {
        let count = FORK_COUNT.fetch_add(1, Ordering::Relaxed) + 1; // spawn counts as a fork+exec

        let mut spawner: PosixSpawner::new(j, dup2s);
        let pid = spawner.spawn(&actual_cmd, &argv, envv);
        if let Some(err) = spawner.get_error() {
            safe_report_exec_error(err, actual_cmd, &argv, envv);
            p.status = ProcStatus::from_exit_code(exit_code_from_exec_error(err));
            return Err(());
        }
        let pid = pid.unwrap();
        assert!(pid > 0, "Should have either a valid pid, or an error");

        // This usleep can be used to test for various race conditions
        // (https://github.com/fish-shell/fish-shell/issues/360).
        // usleep(10000);

        FLOGF!(
            exec_fork,
            "Fork #%d, pid %d: spawn external command '%s' from '%ls'",
            count,
            pid,
            actual_cmd,
            file.unwrap_or("<no file>")
        );

        // these are all things do_fork() takes care of normally (for forked processes):
        p.pid = pid;
        if p.leads_pgrp {
            j.group_mut().set_pgid(p.pid);
            // posix_spawn should in principle set the pgid before returning.
            // In glibc, posix_spawn uses fork() and the pgid group is set on the child side;
            // therefore the parent may not have seen it be set yet.
            // Ensure it gets set. See #4715, also https://github.com/Microsoft/WSL/issues/2997.
            execute_setpgid(p.pid, p.pid, true /* is parent */);
        }
        return Ok(());
    }

    fork_child_for_process(j, p, &dup2s, L!("external command"), |p| {
        safe_launch_process(p, &actual_cmd, &argv, &*envv)
    })
}

// Given that we are about to execute a function, push a function block and set up the
// variable environment.
fn function_prepare_environment(
    parser: &mut Parser,
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
    let vars = parser.vars_mut();

    // Setup the environment for the function. There are three components of the environment:
    // 1. named arguments
    // 2. inherited variables
    // 3. argv

    let mut idx = 0;
    for (idx, named_arg) in props.named_arguments.iter().enumerate() {
        if idx < argv.len() {
            vars.set_one(named_arg, EnvMode::LOCAL | EnvMode::USER, argv[idx].clone());
        } else {
            vars.set_empty(named_arg, EnvMode::LOCAL | EnvMode::USER);
        }
    }

    for (key, value) in &props.inherit_vars {
        vars.set(key, EnvMode::LOCAL | EnvMode::USER, value.clone());
    }

    vars.set_argv(argv);
    fb
}

// Given that we are done executing a function, restore the environment.
fn function_restore_environment(parser: &mut Parser, block: BlockId) {
    parser.pop_block(block);

    // If we returned due to a return statement, then stop returning now.
    parser.libdata_mut().pods.returning = false;
}

// The "performer" function of a block or function process.
// This accepts a place to execute as \p parser and then executes the result, returning a status.
// This is factored out in this funny way in preparation for concurrent execution.
type ProcPerformer = dyn FnOnce(
    &mut Parser,
    &Process,
    Option<&mut dyn OutputStream>,
    Option<&mut dyn OutputStream>,
) -> ProcStatus;

// \return a function which may be to run the given process \p.
// May return an empty std::function in the rare case that the to-be called fish function no longer
// exists. This is just a dumb artifact of the fact that we only capture the functions name, not its
// properties, when creating the job; thus a race could delete the function before we fetch its
// properties.
fn get_performer_for_process(
    p: &Process,
    job: &Job,
    io_chain: &IoChain,
) -> Option<Box<ProcPerformer>> {
    assert!(
        [ProcessType::function, ProcessType::block_node].contains(&p.typ),
        "Unexpected process type"
    );
    // We want to capture the job group.
    let job_group = job.group.clone();
    let io_chain = io_chain.clone();

    if p.typ == ProcessType::block_node {
        Some(Box::new(
            move |parser: &mut Parser, p: &Process, _out, _err| {
                let source = p
                    .block_node_source
                    .as_ref()
                    .expect("Process is missing source info");
                let node = p
                    .internal_block_node
                    .as_ref()
                    .expect("Process is missing node info");
                parser
                    .eval_node(
                        source,
                        unsafe { node.as_ref() },
                        &io_chain,
                        &job_group,
                        BlockType::top,
                    )
                    .status
            },
        ))
    } else {
        assert!(p.typ == ProcessType::function);
        let Some(props) = function_get_props(p.argv0().unwrap()) else {
            FLOGF!(error, "%ls", wgettext_fmt!("Unknown function '%ls'", p.argv0().unwrap()));
            return None;
        };
        Some(Box::new(
            move |parser: &mut Parser, p: &Process, _out, _err| {
                let argv = p.argv();
                // Pull out the job list from the function.
                let props = props.read().unwrap();
                let body = &props.func_node.jobs;
                let fb = function_prepare_environment(parser, argv.clone(), &props);
                let parsed_source = props.parsed_source.as_ref().unwrap();
                let mut res =
                    parser.eval_node(parsed_source, body, &io_chain, &job_group, BlockType::top);
                function_restore_environment(parser, fb);

                // If the function did not execute anything, treat it as success.
                if res.was_empty {
                    res = EvalRes::new(ProcStatus::from_exit_code(EXIT_SUCCESS));
                }
                res.status
            },
        ))
    }
}

/// Execute a block node or function "process".
/// \p piped_output_needs_buffering if true, buffer the output.
fn exec_block_or_func_process(
    parser: &mut Parser,
    j: &JobRef,
    p: &mut Process,
    mut io_chain: IoChain,
    piped_output_needs_buffering: bool,
) -> LaunchResult {
    // Create an output buffer if we're piping to another process.
    let mut block_output_bufferfill = None;
    if piped_output_needs_buffering {
        // Be careful to handle failure, e.g. too many open fds.
        match IoBufferfill::create() {
            Some(tmp) => {
                // Teach the job about its bufferfill, and add it to our io chain.
                io_chain.push(tmp.clone());
                block_output_bufferfill = Some(tmp);
            }
            None => return Err(()),
        }
    }

    // Get the process performer, and just execute it directly.
    // Do it in this scoped way so that the performer function can be eagerly deallocating releasing
    // its captured io chain.
    if let Some(performer) = get_performer_for_process(p, &j.read().unwrap(), &io_chain) {
        p.status = performer(parser, p, None, None);
    } else {
        return Err(());
    }

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
    assert!(p.typ == ProcessType::builtin, "Process must be a builtin");

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
    let argv = p.argv().clone();
    let job_group = j.group.clone();
    let io_chain = io_chain.clone();

    // Be careful to not capture p or j by value, as the intent is that this may be run on another
    // thread.
    Box::new(
        move |parser: &mut Parser,
              p: &Process,
              output_stream: Option<&mut dyn OutputStream>,
              errput_stream: Option<&mut dyn OutputStream>| {
            let output_stream = output_stream.unwrap();
            let errput_stream = errput_stream.unwrap();
            let out_io = io_chain.io_for_fd(STDIN_FILENO);
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

            // Populate our io_streams_t. This is a bag of information for the builtin.
            let mut streams = IoStreams::new(output_stream, errput_stream);
            streams.job_group = job_group;
            streams.stdin_fd = local_builtin_stdin;
            streams.stdin_is_directly_redirected = stdin_is_directly_redirected;
            streams.out_is_redirected = out_io.is_some();
            streams.err_is_redirected = err_io.is_some();
            streams.out_is_piped = out_io
                .map(|io| io.io_mode() == IoMode::pipe)
                .unwrap_or(false);
            streams.err_is_piped = err_io
                .map(|io| io.io_mode() == IoMode::pipe)
                .unwrap_or(false);
            streams.io_chain = &io_chain;

            // Execute the builtin.
            builtin_run(parser, &argv, &streams)
        },
    )
}

/// Executes a builtin "process".
fn exec_builtin_process(
    parser: &mut Parser,
    j: &JobRef,
    p: &mut Process,
    io_chain: &IoChain,
    piped_output_needs_buffering: bool,
) -> LaunchResult {
    assert!(p.typ == ProcessType::builtin, "Process is not a builtin");
    let mut out =
        create_output_stream_for_builtin(STDOUT_FILENO, io_chain, piped_output_needs_buffering);
    let mut err =
        create_output_stream_for_builtin(STDERR_FILENO, io_chain, piped_output_needs_buffering);

    let performer = get_performer_for_builtin(p, &j.read().unwrap(), io_chain);
    p.status = performer(parser, p, Some(&mut *out), Some(&mut *err));
    handle_builtin_output(parser, j, p, io_chain, &*out, &*err);
    Ok(())
}

/// Executes a process \p \p in \p job, using the pipes \p pipes (which may have invalid fds if this
/// is the first or last process).
/// \p deferred_pipes represents the pipes from our deferred process; if set ensure they get closed
/// in any child. If \p is_deferred_run is true, then this is a deferred run; this affects how
/// certain buffering works.
/// An error return here indicates that the process failed to launch, and the rest of
/// the pipeline should be cancelled.
fn exec_process_in_job(
    parser: &mut Parser,
    p: &mut Process,
    j: &JobRef,
    block_io: &IoChain,
    pipes: AutoClosePipes,
    deferred_pipes: &AutoClosePipes,
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
    let mut process_net_io_chain = block_io.clone();

    if pipes.write.is_valid() {
        process_net_io_chain.push(Rc::new(IoPipe::new(
            p.pipe_write_fd,
            false, /* not input */
            pipes.write,
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
    if pipes.read.is_valid() {
        let pipe_read = Rc::new(IoPipe::new(STDIN_FILENO, true /* input */, pipes.read));
        process_net_io_chain.push(pipe_read);
    }

    // If we have stashed pipes, make sure those get closed in the child.
    for afd in [&deferred_pipes.read, &deferred_pipes.write] {
        if afd.is_valid() {
            process_net_io_chain.push(Rc::new(IoClose::new(afd.fd())));
        }
    }

    if p.typ != ProcessType::block_node {
        // A simple `begin ... end` should not be considered an execution of a command.
        parser.libdata_mut().pods.exec_count += 1;
    }

    let mut block_id = None;
    if !p.variable_assignments.is_empty() {
        block_id = Some(parser.push_block(Block::variable_assignment_block()));
    }
    let mut parser = Cleanup::new(parser, |parser| {
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
        ProcessType::function | ProcessType::block_node => exec_block_or_func_process(
            &mut parser,
            j,
            p,
            process_net_io_chain,
            piped_output_needs_buffering,
        ),
        ProcessType::builtin => exec_builtin_process(
            &mut parser,
            j,
            p,
            &process_net_io_chain,
            piped_output_needs_buffering,
        ),
        ProcessType::external => {
            exec_external_command(&mut parser, j, p, &process_net_io_chain)?;
            // It's possible (though unlikely) that this is a background process which recycled a
            // pid from another, previous background process. Forget any such old process.
            parser.mut_wait_handles().remove_by_pid(p.pid);
            Ok(())
        }
        ProcessType::exec => {
            // We should have handled exec up above.
            panic!("process_type_t::exec process found in pipeline, where it should never be. Aborting.");
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
    if j.processes.len() <= 1 {
        return None;
    }

    // Skip execs, which can only appear at the front.
    if j.processes.first().unwrap().typ == ProcessType::exec {
        return None;
    }

    // Find the last non-external process, and return it if it pipes into an extenal process.
    for (i, p) in j.processes.iter().enumerate().rev() {
        if p.typ != ProcessType::external {
            return if p.is_last_in_job { None } else { Some(i) };
        }
    }
    None
}

/// Given that we failed to execute process \p failed_proc in job \p job, mark that process and
/// every subsequent process in the pipeline as aborted before launch.
fn abort_pipeline_from(job: &mut Job, offset: usize) {
    for p in job.processes.iter_mut().skip(offset) {
        p.mark_aborted_before_launch();
    }
}

// Given that we are about to execute an exec() call, check if the parser is interactive and there
// are extant background jobs. If so, warn the user and do not exec().
// \return true if we should allow exec, false to disallow it.
fn allow_exec_with_background_jobs(parser: &mut Parser) -> bool {
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
    let last_exec_run_count = parser.libdata().pods.last_exec_run_counter;
    if unsafe { isatty(STDIN_FILENO) } != 0 && current_run_count - 1 != last_exec_run_count {
        print_exit_warning_for_jobs(&bgs);
        parser.libdata_mut().pods.last_exec_run_counter = current_run_count;
        false
    } else {
        hup_jobs(parser.jobs());
        true
    }
}

/// Populate \p lst with the output of \p buffer, perhaps splitting lines according to \p split.
fn populate_subshell_output(lst: &mut Vec<WString>, buffer: &SeparatedBuffer, split: bool) {
    // Walk over all the elements.
    for elem in buffer.elements() {
        let data = &elem.contents;
        if elem.is_explicitly_separated() {
            // Just append this one.
            lst.push(str2wcstring(data));
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
                let stop = stop.unwrap_or(data.len());
                // Stop now points at the first character we do not want to copy.
                lst.push(str2wcstring(&data[cursor..stop]));

                // If we hit a separator, skip over it; otherwise we're at the end.
                cursor = stop + if hit_separator { 1 } else { 0 };
            }
        } else {
            // We're not splitting output, but we still want to trim off a trailing newline.
            let end = data.iter().rposition(|c| *c != b'\n').unwrap_or(data.len());
            lst.push(str2wcstring(&data[..end]));
        }
    }
}

/// Execute \p cmd in a subshell in \p parser. If \p lst is not null, populate it with the output.
/// Return $status in \p out_status.
/// If \p job_group is set, any spawned commands should join that job group.
/// If \p apply_exit_status is false, then reset $status back to its original value.
/// \p is_subcmd controls whether we apply a read limit.
/// \p break_expand is used to propagate whether the result should be "expansion breaking" in the
/// sense that subshells used during string expansion should halt that expansion. \return the value
/// of $status.
fn exec_subshell_internal(
    cmd: &wstr,
    parser: &mut Parser,
    job_group: &Option<JobGroupRef>,
    lst: Option<&mut Vec<WString>>,
    break_expand: &mut bool,
    apply_exit_status: bool,
    is_subcmd: bool,
) -> libc::c_int {
    parser.assert_can_execute();
    let mut parser = scoped_push(
        parser,
        |parser| &mut parser.libdata_mut().pods.is_subshell,
        true,
    );
    let mut parser = scoped_push(
        parser,
        |parser| &mut parser.libdata_mut().pods.read_limit,
        if is_subcmd {
            READ_BYTE_LIMIT.load(Ordering::Relaxed)
        } else {
            0
        },
    );

    let prev_statuses = parser.get_last_statuses();
    let mut parser = Cleanup::new(parser, |parser| {
        if !apply_exit_status {
            parser.set_last_statuses(prev_statuses);
        }
    });

    let split_output = parser.vars().get_unless_empty(L!("IFS")).is_some();

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may
    // be null.
    let Some(bufferfill) = IoBufferfill::create_opts(parser.libdata().pods.read_limit, STDOUT_FILENO)
        else {
            *break_expand = true;
            return STATUS_CMD_ERROR.unwrap();
    };

    let mut io_chain = IoChain::new();
    io_chain.push(bufferfill.clone());
    let eval_res = parser.eval_with(cmd, &io_chain, job_group, BlockType::subst);
    let buffer = IoBufferfill::finish(bufferfill);
    if buffer.discarded() {
        *break_expand = true;
        return STATUS_READ_TOO_MUCH.unwrap();
    }

    if eval_res.break_expand {
        *break_expand = true;
        return eval_res.status.status_value();
    }

    if let Some(lst) = lst {
        populate_subshell_output(lst, &buffer, split_output);
    }
    *break_expand = false;
    eval_res.status.status_value()
}
