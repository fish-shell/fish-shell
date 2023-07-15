use crate::common::{
    exit_without_destructors, format_llong_safe, format_size_safe, is_windows_subsystem_for_linux,
    narrow_string_safe,
};
use crate::compat::_PATH_BSHELL;
// Functions that we may safely call after fork(), of which there are very few. In particular we
// cannot allocate memory, since we're insane enough to call fork from a multithreaded process.
use crate::exec::{blocked_signals_for_job, is_thompson_shell_script};
use crate::fds::set_cloexec;
use crate::flog::FLOGF_SAFE;
use crate::null_terminated_array::AsNullTerminatedArray;
use crate::proc::{Job, Process};
use crate::redirection::Dup2List;
use crate::signal::{self, signal_reset_handlers};
use errno::{errno, set_errno, Errno};
use libc::{
    c_char, c_int, pid_t, E2BIG, EACCES, EAGAIN, EINTR, EINVAL, EISDIR, ELOOP, EMFILE,
    ENAMETOOLONG, ENFILE, ENOENT, ENOEXEC, ENOMEM, ENOTDIR, EPERM, ESRCH, ETXTBSY, O_RDONLY,
    POSIX_SPAWN_SETPGROUP, POSIX_SPAWN_SETSIGDEF, POSIX_SPAWN_SETSIGMASK, SIGTTIN, SIGTTOU,
    SIG_IGN, SIG_SETMASK, STDIN_FILENO, S_IFDIR, S_IFMT, X_OK, _SC_ARG_MAX,
};
use std::ffi::CStr;
use std::time::Duration;

/// The number of times to try to call fork() before giving up.
const FORK_LAPS: usize = 5;

/// The number of milliseconds to sleep between attempts to call fork().
const FORK_SLEEP_TIME: Duration = Duration::from_millis(1);

/// Returns the interpreter for the specified script. Returns NULL if file is not a script with a
/// shebang.
fn get_interpreter<'a>(command: &CStr, buffer: &'a mut [u8]) -> Option<&'a CStr> {
    // OK to not use CLO_EXEC here because this is only called after fork.
    let fd = unsafe { libc::open(command.as_ptr(), O_RDONLY) };
    let mut idx = 0;
    if fd >= 0 {
        while idx + 1 < buffer.len() {
            let mut ch = b'\0';
            let amt = unsafe {
                libc::read(
                    fd,
                    std::ptr::addr_of_mut!(ch).cast(),
                    std::mem::size_of_val(&ch),
                )
            };
            if amt <= 0 || ch == b'\n' {
                break;
            }
            buffer[idx] = ch;
            idx += 1;
        }
        buffer[idx] = b'\0';
        idx += 1;
        unsafe { libc::close(fd) };
    }

    let offset = if buffer.starts_with(b"#! /") {
        3
    } else if buffer.starts_with(b"#!/") ||
    // Relative path, basically always an issue.
    buffer.starts_with(b"#!")
    {
        2
    } else {
        return None;
    };
    Some(CStr::from_bytes_with_nul(&buffer[offset..idx.max(offset)]).unwrap())
}

#[test]
fn foo() {
    let a = b"123";
    let b = &a[2..1];
    assert!(b.len() == 0);
}

/// Tell the proc \p pid to join process group \p pgroup.
/// If \p is_child is true, we are the child process; otherwise we are fish.
/// Called by both parent and child; this is an unavoidable race inherent to Unix.
/// If is_parent is set, then we are the parent process and should swallow EACCESS.
/// \return 0 on success, an errno error code on failure.
pub fn execute_setpgid(pid: pid_t, pgroup: pid_t, is_parent: bool) -> c_int {
    // Historically we have looped here to support WSL.
    let mut eperm_count = 0;
    loop {
        if unsafe { libc::setpgid(pid, pgroup) } == 0 {
            return 0;
        }
        let err = errno().0;
        if err == EACCES && is_parent {
            // We are the parent process and our child has called exec().
            // This is an unavoidable benign race.
            return 0;
        } else if err == EINTR {
            // Paranoia.
            continue;
        } else if err == EPERM {
            eperm_count += 1;
            if eperm_count < 100 {
                // The setpgid(2) man page says that EPERM is returned only if attempts are made
                // to move processes into groups across session boundaries (which can never be
                // the case in fish, anywhere) or to change the process group ID of a session
                // leader (again, can never be the case). I'm pretty sure this is a WSL bug, as
                // we see the same with tcsetpgrp(2) in other places and it disappears on retry.
                FLOGF_SAFE!(proc_pgroup, "setpgid(2) returned EPERM. Retrying");
                continue;
            }
        }
        #[cfg(any(feature = "bsd", target_os = "macos"))]
        // POSIX.1 doesn't specify that zombie processes are required to be considered extant and/or
        // children of the parent for purposes of setpgid(2). In particular, FreeBSD (at least up to
        // 12.2) does not consider a child that has already forked, exec'd, and exited to "exist"
        // and returns ESRCH (process not found) instead of EACCES (child has called exec).
        // See https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=251227
        if err == ESRCH && is_parent {
            // Handle this just like we would EACCES above, as we're virtually certain that
            // setpgid(2) was called against a process that was at least at one point in time a
            // valid child.
            return 0;
        }
        return err;
    }
}

/// Report the error code \p err for a failed setpgid call.
/// Note not all errors should be reported; in particular EACCESS is expected and benign in the
/// parent only.
pub fn report_setpgid_error(
    err: c_int,
    is_parent: bool,
    desired_pgid: pid_t,
    j: &Job,
    p: &Process,
) {
    let mut pid_buff = [b'0'; 64];
    let mut job_id_buff = [b'0'; 64];
    let mut getpgid_buff = [b'0'; 64];
    let mut job_pgid_buff = [b'0'; 64];
    let mut argv0 = [b'0'; 64];
    let mut command = [b'0'; 64];

    format_llong_safe(&mut pid_buff, p.pid());
    format_llong_safe(&mut job_id_buff, j.job_id().as_num());
    format_llong_safe(&mut getpgid_buff, unsafe { libc::getpgid(p.pid()) });
    format_llong_safe(&mut job_pgid_buff, desired_pgid);
    narrow_string_safe(&mut argv0, p.argv0().unwrap());
    narrow_string_safe(&mut command, j.command());

    FLOGF_SAFE!(
        warning,
        "Could not send %s %s, '%s' in job %s, '%s' from group %s to group %s",
        if is_parent { "child" } else { "self" },
        pid_buff,
        argv0,
        job_id_buff,
        command,
        getpgid_buff,
        job_pgid_buff
    );

    if is_windows_subsystem_for_linux() && errno().0 == EPERM {
        FLOGF_SAFE!(
            warning,
            concat!("Please update to Windows 10 1809/17763 or higher to address known issues "
                   "with process groups and zombie processes.")
        );
    }

    set_errno(Errno(err));

    let setpgid_errno = errno().0;
    match setpgid_errno {
        EACCES => {
            FLOGF_SAFE!(error, "setpgid: Process %s has already exec'd", pid_buff);
        }
        EINVAL => {
            FLOGF_SAFE!(error, "setpgid: pgid %s unsupported", getpgid_buff);
        }
        EPERM => {
            FLOGF_SAFE!(
                error,
                "setpgid: Process %s is a session leader or pgid %s does not match",
                pid_buff,
                getpgid_buff
            );
        }
        ESRCH => {
            FLOGF_SAFE!(error, "setpgid: Process ID %s does not match", pid_buff);
        }
        _ => {
            let mut errno_buff = [b'\0'; 64];
            format_llong_safe(&mut errno_buff, setpgid_errno);
            FLOGF_SAFE!(error, "setpgid: Unknown error number %s", errno_buff);
        }
    }
}

/// Initialize a new child process. This should be called right away after forking in the child
/// process. This resets signal handlers and applies IO redirections.
///
/// If \p claim_tty_from is >= 0 and owns the tty, use tcsetpgrp() to claim it.
///
/// \return 0 on success, -1 on failure, in which case an error will be printed.
pub fn child_setup_process(
    claim_tty_from: pid_t,
    job: &Job,
    is_forked: bool,
    dup2s: &Dup2List,
) -> c_int {
    // Note we are called in a forked child.
    for act in dup2s.get_actions() {
        let err = if act.target < 0 {
            unsafe { libc::close(act.src) }
        } else if act.target != act.src {
            // Normal redirection.
            unsafe { libc::dup2(act.src, act.target) }
        } else {
            // This is a weird case like /bin/cmd 6< file.txt
            // The opened file (which is CLO_EXEC) wants to be dup2'd to its own fd.
            // We need to unset the CLO_EXEC flag.
            set_cloexec(act.src, false)
        };
        if err < 0 {
            if is_forked {
                FLOGF_SAFE!(
                    warning,
                    "failed to set up file descriptors in child_setup_process",
                );
                exit_without_destructors(1);
            }
            return err;
        }
    }
    if claim_tty_from >= 0 && unsafe { libc::tcgetpgrp(STDIN_FILENO) } == claim_tty_from {
        // Assign the terminal within the child to avoid the well-known race between tcsetgrp() in
        // the parent and the child executing. We are not interested in error handling here, except
        // we try to avoid this for non-terminals; in particular pipelines often make non-terminal
        // stdin.
        // Only do this if the tty currently belongs to fish's pgrp. Don't try to steal it away from
        // another process which may happen if we are run in the background with job control
        // enabled. Note if stdin is not a tty, then tcgetpgrp() will return -1 and we will not
        // enter this.
        // Ensure this doesn't send us to the background (see #5963)
        unsafe {
            libc::signal(SIGTTIN, SIG_IGN);
            libc::signal(SIGTTOU, SIG_IGN);
            let _ = libc::tcsetpgrp(STDIN_FILENO, libc::getpid());
        }
    }
    let mut sigmask: libc::sigset_t = unsafe { std::mem::zeroed() };
    unsafe { libc::sigemptyset(&mut sigmask) };
    if blocked_signals_for_job(job, &mut sigmask) {
        unsafe { libc::sigprocmask(SIG_SETMASK, &sigmask, std::ptr::null_mut()) };
    }
    // Set the handling for job control signals back to the default.
    // Do this after any tcsetpgrp call so that we swallow SIGTTIN.
    signal_reset_handlers();
    0
}

/// Call fork(), retrying on failure a few times.
/// This function is a wrapper around fork. If the fork calls fails with EAGAIN, it is retried
/// FORK_LAPS times, with a very slight delay between each lap. If fork fails even then, the process
/// will exit with an error message.
pub fn execute_fork() -> pid_t {
    #[cfg(feature = "JOIN_THREADS_BEFORE_FORK")]
    {
        // Make sure we have no outstanding threads before we fork. This is a pretty sketchy thing
        // to do here, both because exec.cpp shouldn't have to know about iothreads, and because the
        // completion handlers may do unexpected things.
        FLOGF_SAFE!(iothread, "waiting for threads to drain.");
        iothread_drain_all();
    }

    for i in 0..FORK_LAPS {
        let pid = unsafe { libc::fork() };
        if pid >= 0 {
            return pid;
        }

        if errno().0 != EAGAIN {
            break;
        }

        // Don't sleep on the final lap - sleeping might change the value of errno, which will break
        // the error reporting below.
        if i != FORK_LAPS - 1 {
            std::thread::sleep(FORK_SLEEP_TIME);
        }
    }

    // These are all the errno numbers for fork() I can find.
    // Also ENOSYS, but I doubt anyone is running
    // fish on a platform without an MMU.
    let fork_errno = errno().0;
    match fork_errno {
        EAGAIN => {
            // We should have retried these already?
            FLOGF_SAFE!(
                error,
                "fork: Out of resources. Check RLIMIT_NPROC and pid_max."
            );
        }
        ENOMEM => {
            FLOGF_SAFE!(error, "fork: Out of memory.");
        }
        _ => {
            let mut errno_buff = [b'\0'; 64];
            format_llong_safe(&mut errno_buff, fork_errno);
            FLOGF_SAFE!(error, "fork: Unknown error number %s", errno_buff);
        }
    }
    panic!();
}

/// Report an error from failing to exec or posix_spawn a command.
pub fn safe_report_exec_error(
    err: Errno,
    actual_cmd: &CStr,
    argv: &impl AsNullTerminatedArray<CharType = c_char>,
    envv: &impl AsNullTerminatedArray<CharType = c_char>,
) {
    match err.0 {
        E2BIG => {
            let mut sz1 = [b'\0'; 128];

            let mut sz = 0;
            let mut szenv = 0;
            for p in argv.iter() {
                sz += unsafe { CStr::from_ptr(p) }.to_bytes().len();
            }
            for p in envv.iter() {
                szenv += unsafe { CStr::from_ptr(p) }.to_bytes().len();
            }
            sz += szenv;

            format_size_safe(&mut sz1, sz.try_into().unwrap());
            let arg_max = unsafe { libc::sysconf(_SC_ARG_MAX) };

            if arg_max > 0 {
                if sz >= arg_max as usize {
                    let mut sz2 = [b'\0'; 128];
                    format_size_safe(&mut sz2, arg_max as u64);
                    FLOGF_SAFE!(
                        exec,
                        concat!("Failed to execute process '%s': the total size of the argument list and "
                        "exported variables (%s) exceeds the OS limit of %s."),
                        actual_cmd,
                        sz1,
                        sz2
                    );
                } else {
                    // MAX_ARG_STRLEN, a linux thing that limits the size of one argument. It's
                    // defined in binfmts.h, but we don't want to include that just to be able to
                    // print the real limit.
                    FLOGF_SAFE!(
                        exec,
                        concat!("Failed to execute process '%s': An argument or exported variable "
                               "exceeds the OS "
                               "argument length limit."),
                        actual_cmd
                    );
                }

                if szenv >= arg_max as usize / 2 {
                    FLOGF_SAFE!(
                        exec,
                        concat!("Hint: Your exported variables take up over half the limit. Try "
                               "erasing or unexporting variables.")
                    );
                }
            } else {
                FLOGF_SAFE!(
                    exec,
                    concat!("Failed to execute process '%s': the total size of the argument list and "
                    "exported variables (%s) exceeds the "
                    "operating system limit."),
                    actual_cmd,
                    sz1
                );
            }
        }

        ENOEXEC => {
            FLOGF_SAFE!(
                exec,
                concat!("Failed to execute process: '%s' the file could not be run by the "
                       "operating system."),
                actual_cmd
            );
            let mut interpreter_buf = [b'\0'; 128];
            if get_interpreter(actual_cmd, &mut interpreter_buf).is_none() {
                // Paths ending in ".fish" need to start with a shebang
                if actual_cmd.to_bytes().ends_with(b".fish") {
                    FLOGF_SAFE!(
                        exec,
                        concat!("fish scripts require an interpreter directive (must "
                                   "start with '#!/path/to/fish').")
                    );
                }
            } else {
                // If the shebang line exists, we would get an ENOENT or similar instead,
                // so I don't know how to reach this.
                FLOGF_SAFE!(
                    exec,
                    "Maybe the interpreter directive (#! line) is broken?",
                    actual_cmd
                );
            }
        }

        EACCES | ENOENT => {
            // ENOENT is returned by exec() when the path fails, but also returned by posix_spawn if
            // an open file action fails. These cases appear to be impossible to distinguish. We
            // address this by not using posix_spawn for file redirections, so all the ENOENTs we
            // find must be errors from exec().
            let mut interpreter_buf = [b'\0'; 128];
            if let Some(interpreter) = get_interpreter(actual_cmd, &mut interpreter_buf) {
                let mut buf: libc::stat = unsafe { std::mem::zeroed() };
                let statret = unsafe { libc::stat(interpreter.as_ptr(), &mut buf) };
                if statret != 0 || unsafe { libc::access(interpreter.as_ptr(), X_OK) } != 0 {
                    // Detect Windows line endings and complain specifically about them.
                    let interpreter = interpreter.to_bytes();
                    if interpreter.last() == Some(&b'\r') {
                        FLOGF_SAFE!(
                            exec,
                            concat!("Failed to execute process '%s':  The file uses Windows line "
                               "endings (\\r\\n). Run dos2unix or similar to fix it."),
                            actual_cmd
                        );
                    } else {
                        FLOGF_SAFE!(
                            exec,
                            concat!("Failed to execute process '%s': The file specified the interpreter "
                               "'%s', which is not an "
                               "executable command."),
                            actual_cmd,
                            interpreter
                        );
                    }
                } else if buf.st_mode & S_IFMT == S_IFDIR {
                    FLOGF_SAFE!(
                        exec,
                        concat!("Failed to execute process '%s': The file specified the interpreter "
                           "'%s', which is a directory."),
                        actual_cmd,
                        interpreter
                    );
                }
            } else if unsafe { libc::access(actual_cmd.as_ptr(), X_OK) } == 0 {
                FLOGF_SAFE!(
                    exec,
                    concat!("Failed to execute process '%s': The file exists and is executable. "
                           "Check the interpreter or linker?"),
                    actual_cmd
                );
            } else if err.0 == ENOENT {
                FLOGF_SAFE!(
                    exec,
                    concat!("Failed to execute process '%s': The file does not exist or could not "
                           "be executed."),
                    actual_cmd
                );
            } else {
                FLOGF_SAFE!(
                    exec,
                    "Failed to execute process '%s': The file could not be accessed.",
                    actual_cmd
                );
            }
        }
        ENOMEM => {
            FLOGF_SAFE!(exec, "Out of memory");
        }
        ETXTBSY => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': File is currently open for writing.",
                actual_cmd
            );
        }
        ELOOP => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': Too many layers of symbolic links. Maybe a loop?",
                actual_cmd
            );
        }
        EINVAL => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': Unsupported format.",
                actual_cmd
            );
        }
        EISDIR => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': File is a directory.",
                actual_cmd
            );
        }
        ENOTDIR => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': A path component is not a directory.",
                actual_cmd
            );
        }

        EMFILE => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': Too many open files in this process.",
                actual_cmd
            );
        }
        ENFILE => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': Too many open files on the system.",
                actual_cmd
            );
        }
        ENAMETOOLONG => {
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s': Name is too long.",
                actual_cmd
            );
        }
        EPERM => {
            FLOGF_SAFE!(
                exec,
                concat!(
                    "Failed to execute process '%s': No permission. Either suid/sgid is "
                    "forbidden or you lack capabilities."
                ),
                actual_cmd
            );
        }
        _ => {
            #[cfg(target_os = "macos")]
            if err.0 == EBADARCH {
                FLOGF_SAFE!(
                    exec,
                    "Failed to execute process '%s': Bad CPU type in executable.",
                    actual_cmd
                );
                return;
            }
            let mut errnum_buff = [b'\0'; 64];
            format_llong_safe(&mut errnum_buff, err.0);
            FLOGF_SAFE!(
                exec,
                "Failed to execute process '%s', unknown error number %s",
                actual_cmd,
                errnum_buff
            );
        }
    }
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
        let mut zelf = Self {};
        // Initialize our fields. This may fail.
        {
            let mut attr: libc::posix_spawnattr_t = unsafe { std::mem::zeroed() };
            if zelf.check_fail(unsafe { posix_spawnattr_init(&mut attr) }) {
                return zelf;
            }
            zelf.attr = Some(attr);
        }
        {
            let mut actions: libc::posix_spawn_file_actions_t = unsafe { std::mem::zeroed() };
            if !zelf.check_fail(unsafe { posix_spawn_file_actions_init(&mut actions) }) {
                return zelf;
            }
            zelf.actions = Some(actions);
        }

        // desired_pgid tracks the pgroup for the process. If it is none, the pgroup is left unchanged.
        // If it is zero, create a new pgroup from the pid. If it is >0, join that pgroup.
        let mut desired_pgid = None;
        if let Some(pgid) = j.group().get_pgid() {
            desired_pgid = Some(pgid);
        } else if j.processes.first().unwrap().leads_pgrp {
            desired_pgid = Some(0);
        }

        // Set the handling for job control signals back to the default.
        let reset_signal_handlers = true;

        // Remove all signal blocks.
        let reset_sigmask = true;

        // Set our flags.
        let flags = 0;
        if reset_signal_handlers {
            flags |= POSIX_SPAWN_SETSIGDEF;
        }
        if reset_sigmask {
            flags |= POSIX_SPAWN_SETSIGMASK;
        }
        if desired_pgid.is_some() {
            flags |= POSIX_SPAWN_SETPGROUP;
        }

        if zelf.check_fail(unsafe { libc::posix_spawnattr_setflags(zelf.attr_mut(), flags) }) {
            return zelf;
        }

        if let Some(desired_pgid) = desired_pgid {
            if zelf.check_fail(unsafe {
                libc::posix_spawnattr_setpgroup(zelf.attr_mut(), desired_pgid)
            }) {
                return zelf;
            }
        }

        // Everybody gets default handlers.
        if reset_signal_handlers {
            let mut sigdefault: libc::sigset_t = unsafe { std::mem::zeroed() };
            get_signals_with_handlers(&mut sigdefault);
            if zelf.check_fail(unsafe {
                libc::posix_spawnattr_setsigdefault(zelf.attr_mut(), &sigdefault)
            }) {
                return zelf;
            }
        }

        // No signals blocked.
        if reset_sigmask {
            let mut sigmask: libc::sigset_t = unsafe { std::mem::zeroed() };
            unsafe { libc::sigemptyset(&mut sigmask) };
            blocked_signals_for_job(j, &mut sigmask);
            if zelf
                .check_fail(unsafe { libc::posix_spawnattr_setsigmask(zelf.attr_mut(), &sigmask) })
            {
                return zelf;
            }
        }

        // Apply our dup2s.
        for act in dup2s.get_actions() {
            if act.target < 0 {
                if zelf.check_fail(unsafe {
                    libc::posix_spawn_file_actions_addclose(zelf.actions_mut(), act.src)
                }) {
                    return zelf;
                }
            } else {
                if zelf.check_fail(unsafe {
                    libc::posix_spawn_file_actions_adddup2(zelf.actions_mut(), act.src, act.target)
                }) {
                    return zelf;
                }
            }
        }

        zelf
    }

    /// \return the last error code, or 0 if there is no error.
    pub fn get_error(&self) -> c_int {
        self.error
    }

    /// If this spawner does not have an error, invoke posix_spawn. Parameters are the same as
    /// posix_spawn.
    /// \return the pid, or none() on failure, in which case our error will be set.
    pub fn spawn(
        &mut self,
        cmd: &CStr,
        argv: &impl AsNullTerminatedArray<CharType = c_char>,
        envp: &impl AsNullTerminatedArray<CharType = c_char>,
    ) -> Option<pid_t> {
        if self.get_error() != 0 {
            return None;
        }
        let mut pid: pid_t = -1;
        if self.check_fail(unsafe {
            libc::posix_spawn(&mut pid, cmd, self.actions(), self.attr(), argv, envp)
        }) {
            // The shebang wasn't introduced until UNIX Seventh Edition, so if
            // the kernel won't run the binary we hand it off to the interpreter
            // after performing a binary safety check, recommended by POSIX: a
            // line needs to exist before the first \0 with a lowercase letter
            if self.error == ENOEXEC && is_thompson_shell_script(cmd) {
                // Create a new argv with /bin/sh prepended.
                let mut argv2 = vec![];
                let interp = &*_PATH_BSHELL;
                argv2.push(interp);
                // The command to call should use the full path,
                // not what we would pass as argv0.
                let cmd2 = cmd;
                argv2.push(cmd2);
                for arg in argv.iter().skip(1) {
                    argv2.push(arg);
                }
                argv2.push(std::ptr::null());
                let argv2 = AsNullTerminatedArray::new(&argv2);
                if self.check_fail(unsafe {
                    posix_spawn(
                        &mut pid,
                        interp.as_ptr(),
                        self.actions(),
                        self.attr(),
                        &argv2,
                        envp,
                    )
                }) {
                    return None;
                }
            } else {
                return None;
            }
        }
        Some(pid)
    }

    fn check_fail(&mut self, err: c_int) -> bool {
        if self.error == 0 {
            self.error = err;
        }
        self.error != 0
    }
    fn attr(&self) -> &libc::posix_spawn_attr_t {
        self.attr.as_ref().unwrap()
    }
    fn attr_mut(&mut self) -> &mut libc::posix_spawn_attr_t {
        self.attr.as_mut().unwrap()
    }
    fn actions(&self) -> &libc::posix_spawn_file_actions_t {
        self.actions.as_ref().unwrap()
    }
    fn actions_mut(&mut self) -> &mut libc::posix_spawn_file_actions_t {
        self.actions.as_mut().unwrap()
    }
}

#[cfg(FISH_USE_POSIX_SPAWN)]
impl Drop for PosixSpawner {
    fn drop(&mut self) {
        if let Some(attr) = self.attr {
            unsafe {
                libc::posix_spawnattr_destroy(&attr);
            }
        }
        if let Some(actions) = self.actions {
            unsafe {
                libc::posix_spawn_file_actions_destroy(&actions);
            }
        }
    }
}
