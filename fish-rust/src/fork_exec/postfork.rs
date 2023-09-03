// The stuff that happens after fork.
// Everything in this module must be async-signal safe.
// That means no locking, no allocating, no freeing memory, etc!
use super::flog_safe::FLOG_SAFE;
use crate::common::exit_without_destructors;
use crate::redirection::Dup2List;
use crate::signal::signal_reset_handlers;
use libc::{c_char, c_int, pid_t};
use std::ffi::CStr;

/// The number of times to try to call fork() before giving up.
const FORK_LAPS: usize = 5;

/// The number of nanoseconds to sleep between attempts to call fork().
const FORK_SLEEP_TIME: libc::c_long = 1000000;

/// Clear FD_CLOEXEC on a file descriptor.
fn clear_cloexec(fd: i32) -> i32 {
    // Note we don't want to overwrite existing flags like O_NONBLOCK which may be set. So fetch the
    // existing flags and modify them.
    let flags = unsafe { libc::fcntl(fd, libc::F_GETFD, 0) };
    if flags < 0 {
        return -1;
    }
    let new_flags = flags & !libc::FD_CLOEXEC;
    if flags == new_flags {
        return 0;
    } else {
        return unsafe { libc::fcntl(fd, libc::F_SETFD, new_flags) };
    }
}

/// Safe strlen(). Note strlen is async-signal safe as of POSIX.1-2008
/// but we just implement our own.
fn strlen_safe(s: *const libc::c_char) -> usize {
    let mut len = 0;
    let mut cursor: *const libc::c_char = s;
    unsafe {
        while *cursor != 0 {
            len += 1;
            cursor = cursor.offset(1);
        }
    }
    len
}

/// Report the error code for a failed setpgid call.
pub fn report_setpgid_error(
    err: i32,
    is_parent: bool,
    pid: pid_t,
    desired_pgid: pid_t,
    job_id: c_int,
    command_str: *const c_char,
    argv0_str: *const c_char,
) {
    let cur_group = unsafe { libc::getpgid(pid) };

    // TODO: command_str ought to be passed as a CStr.
    // This is worth fixing as CStr::from_ptr may invoke libc strlen() which
    // is not async signal safe on old versions of POSIX.
    let command: &CStr = unsafe { CStr::from_ptr(command_str) };
    let argv0: &CStr = unsafe { CStr::from_ptr(argv0_str) };

    FLOG_SAFE!(
        warning,
        "Could not send ",
        if is_parent { "child" } else { "self" },
        " ",
        pid,
        ", '",
        argv0,
        "' in job ",
        job_id,
        ", '",
        command,
        "' from group ",
        cur_group,
        " to group ",
        desired_pgid,
    );

    match err {
        libc::EACCES => FLOG_SAFE!(error, "setpgid: Process ", pid, " has already exec'd"),
        libc::EINVAL => FLOG_SAFE!(error, "setpgid: pgid ", cur_group, " unsupported"),
        libc::EPERM => {
            FLOG_SAFE!(
                error,
                "setpgid: Process ",
                pid,
                " is a session leader or pgid ",
                cur_group,
                " does not match"
            );
        }
        libc::ESRCH => FLOG_SAFE!(error, "setpgid: Process ID ", pid, " does not match"),
        _ => FLOG_SAFE!(error, "setpgid: Unknown error number ", err),
    }
}

/// Execute setpgid, moving pid into the given pgroup.
/// Return the result of setpgid.
fn execute_setpgid(pid: pid_t, pgroup: pid_t, is_parent: bool) -> i32 {
    // There is a comment "Historically we have looped here to support WSL."
    // TODO: stop looping.
    let mut eperm_count = 0;
    loop {
        if unsafe { libc::setpgid(pid, pgroup) } == 0 {
            return 0;
        }
        let err = errno::errno().0;
        if err == libc::EACCES && is_parent {
            // We are the parent process and our child has called exec().
            // This is an unavoidable benign race.
            return 0;
        } else if err == libc::EINTR {
            // Paranoia.
            continue;
        } else if err == libc::EPERM && eperm_count < 100 {
            eperm_count += 1;
            // The setpgid(2) man page says that EPERM is returned only if attempts are made
            // to move processes into groups across session boundaries (which can never be
            // the case in fish, anywhere) or to change the process group ID of a session
            // leader (again, can never be the case). I'm pretty sure this is a WSL bug, as
            // we see the same with tcsetpgrp(2) in other places and it disappears on retry.
            FLOG_SAFE!(proc_pgroup, "setpgid(2) returned EPERM. Retrying");
            continue;
        }

        // POSIX.1 doesn't specify that zombie processes are required to be considered extant and/or
        // children of the parent for purposes of setpgid(2). In particular, FreeBSD (at least up to
        // 12.2) does not consider a child that has already forked, exec'd, and exited to "exist"
        // and returns ESRCH (process not found) instead of EACCES (child has called exec).
        // See https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=251227
        #[cfg(any(feature = "bsd", target_os = "macos"))]
        if err == libc::ESRCH && is_parent {
            // Handle this just like we would EACCES above, as we're virtually certain that
            // setpgid(2) was called against a process that was at least at one point in time a
            // valid child.
            return 0;
        }

        return err;
    }
}

/// Set up redirections and signal handling in the child process.
fn child_setup_process(
    claim_tty_from: pid_t,
    sigmask: Option<&libc::sigset_t>,
    is_forked: bool,
    dup2s: &Dup2List,
) -> i32 {
    // Note we are called in a forked child.
    for act in &dup2s.actions {
        let err;
        if act.target < 0 {
            err = unsafe { libc::close(act.src) };
        } else if act.target != act.src {
            // Normal redirection.
            err = unsafe { libc::dup2(act.src, act.target) };
        } else {
            // This is a weird case like /bin/cmd 6< file.txt
            // The opened file (which is CLO_EXEC) wants to be dup2'd to its own fd.
            // We need to unset the CLO_EXEC flag.
            err = clear_cloexec(act.src);
        }
        if err < 0 {
            if is_forked {
                FLOG_SAFE!(
                    warning,
                    "failed to set up file descriptors in child_setup_process"
                );
                exit_without_destructors(1);
            }
            return err;
        }
    }
    if claim_tty_from >= 0 && unsafe { libc::tcgetpgrp(libc::STDIN_FILENO) } == claim_tty_from {
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
            libc::signal(libc::SIGTTIN, libc::SIG_IGN);
            libc::signal(libc::SIGTTOU, libc::SIG_IGN);
            let _ = libc::tcsetpgrp(libc::STDIN_FILENO, libc::getpid());
        }
    }
    if let Some(sigmask) = sigmask {
        unsafe { libc::sigprocmask(libc::SIG_SETMASK, sigmask, std::ptr::null_mut()) };
    }
    // Set the handling for job control signals back to the default.
    // Do this after any tcsetpgrp call so that we swallow SIGTTIN.
    signal_reset_handlers();
    0
}

/// This function is a wrapper around fork. If the fork calls fails with EAGAIN, it is retried
/// FORK_LAPS times, with a very slight delay between each lap. If fork fails even then, the process
/// will exit with an error message.
fn execute_fork() -> pid_t {
    let mut err = 0;
    for i in 0..FORK_LAPS {
        let pid = unsafe { libc::fork() };
        if pid >= 0 {
            return pid;
        }
        err = errno::errno().0;
        if err != libc::EAGAIN {
            break;
        }
        let pollint = libc::timespec {
            tv_sec: 0,
            tv_nsec: FORK_SLEEP_TIME,
        };
        // Don't sleep on the final lap
        if i != FORK_LAPS - 1 {
            unsafe { libc::nanosleep(&pollint, std::ptr::null_mut()) };
        }
    }

    match err {
        libc::EAGAIN => {
            FLOG_SAFE!(
                error,
                "fork: Out of resources. Check RLIMIT_NPROC and pid_max."
            );
        }
        libc::ENOMEM => {
            FLOG_SAFE!(error, "fork: Out of memory.");
        }
        _ => {
            FLOG_SAFE!(error, "fork: Unknown error number", err);
        }
    }
    exit_without_destructors(1)
}

fn safe_report_exec_error(
    err: i32,
    actual_cmd: *const c_char,
    argvv: *const *const c_char,
    envv: *const *const c_char,
) {
    // TODO: actual_cmd may be passed as a CStr.
    let actual_cmd: &CStr = unsafe { CStr::from_ptr(actual_cmd) };
    match err {
        libc::E2BIG => {
            let mut sz = 0;
            let mut szenv = 0;
            unsafe {
                // Compute total size of argv.
                let mut cursor = argvv;
                while !(*cursor).is_null() {
                    sz += strlen_safe(*cursor) + 1;
                    cursor = cursor.offset(1);
                }

                // Compute total size of envp.
                cursor = envv;
                while !(*cursor).is_null() {
                    szenv += strlen_safe(*cursor) + 1;
                    cursor = cursor.offset(1);
                }
                sz += szenv;
            }

            let arg_max = unsafe { libc::sysconf(libc::_SC_ARG_MAX) };
            if arg_max > 0 {
                let arg_max = arg_max as usize;
                if sz >= arg_max {
                    FLOG_SAFE!(
                        exec,
                        "Failed to execute process '",
                        actual_cmd,
                        "': the total size of the argument list and exported variables (",
                        sz,
                        ") exceeds the OS limit of ",
                        arg_max,
                        "."
                    );
                } else {
                    // MAX_ARG_STRLEN, a linux thing that limits the size of one argument. It's
                    // defined in binfmts.h, but we don't want to include that just to be able to
                    // print the real limit.
                    FLOG_SAFE!(
                        exec,
                        "Failed to execute process '",
                        actual_cmd,
                        "': An argument or exported variable exceeds the OS argument length limit."
                    );
                }

                if szenv >= arg_max / 2 {
                    FLOG_SAFE!(
                        exec,
                        "Hint: Your exported variables take up over half the limit. Try \
                         erasing or unexporting variables."
                    );
                }
            } else {
                FLOG_SAFE!(
                    exec,
                    "Failed to execute process '",
                    actual_cmd,
                    "': the total size of the argument list and exported variables (",
                    sz,
                    ") exceeds the operating system limit.",
                );
            }
        }

        libc::ENOEXEC => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process: '",
                actual_cmd,
                "' the file could not be run by the operating system."
            );
            let mut interpreter_buf = [b'\0'; 128];
            if get_interpreter(actual_cmd, &mut interpreter_buf).is_none() {
                // Paths ending in ".fish" need to start with a shebang
                if actual_cmd.to_bytes().ends_with(b".fish") {
                    FLOG_SAFE!(
                        exec,
                        "fish scripts require an interpreter directive (must \
                         start with '#!/path/to/fish')."
                    );
                } else {
                    // If the shebang line exists, we would get an ENOENT or similar instead,
                    // so I don't know how to reach this.
                    FLOG_SAFE!(exec, "Maybe the interpreter directive (#! line) is broken?");
                }
            }
        }
        libc::EACCES | libc::ENOENT => {
            // ENOENT is returned by exec() when the path fails, but also returned by posix_spawn if
            // an open file action fails. These cases appear to be impossible to distinguish. We
            // address this by not using posix_spawn for file redirections, so all the ENOENTs we
            // find must be errors from exec().
            let mut interpreter_buf = [b'\0'; 128];
            if let Some(interpreter) = get_interpreter(actual_cmd, &mut interpreter_buf) {
                let mut buf: libc::stat = unsafe { std::mem::zeroed() };
                let statret = unsafe { libc::stat(interpreter.as_ptr(), &mut buf) };
                if statret != 0 || unsafe { libc::access(interpreter.as_ptr(), libc::X_OK) } != 0 {
                    // Detect Windows line endings and complain specifically about them.
                    let interpreter = interpreter.to_bytes();
                    if interpreter.last() == Some(&b'\r') {
                        FLOG_SAFE!(
                            exec,
                            "Failed to execute process '",
                            actual_cmd,
                            "':  The file uses Windows line endings (\\r\\n). Run dos2unix or similar to fix it."
                        );
                    } else {
                        FLOG_SAFE!(
                            exec,
                            "Failed to execute process '",
                            actual_cmd,
                            "': The file specified the interpreter '",
                            interpreter,
                            "', which is not an executable command."
                        );
                    }
                } else if buf.st_mode & libc::S_IFMT == libc::S_IFDIR {
                    FLOG_SAFE!(
                        exec,
                        "Failed to execute process '",
                        actual_cmd,
                        "': The file specified the interpreter '",
                        interpreter,
                        "', which is a directory."
                    );
                }
            } else if unsafe { libc::access(actual_cmd.as_ptr(), libc::X_OK) } == 0 {
                FLOG_SAFE!(
                    exec,
                    "Failed to execute process '",
                    actual_cmd,
                    "': The file exists and is executable. Check the interpreter or linker?"
                );
            } else if err == libc::ENOENT {
                FLOG_SAFE!(
                    exec,
                    "Failed to execute process '",
                    actual_cmd,
                    "': The file does not exist or could not be executed."
                );
            } else {
                FLOG_SAFE!(
                    exec,
                    "Failed to execute process '",
                    actual_cmd,
                    "': The file could not be accessed."
                );
            }
        }

        libc::ENOMEM => {
            FLOG_SAFE!(exec, "Out of memory");
        }

        libc::ETXTBSY => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': File is currently open for writing.",
            );
        }

        libc::ELOOP => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': Too many layers of symbolic links. Maybe a loop?"
            );
        }

        libc::EINVAL => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': Unsupported format."
            );
        }
        libc::EISDIR => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': File is a directory."
            );
        }
        libc::ENOTDIR => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': A path component is not a directory."
            );
        }

        libc::EMFILE => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': Too many open files in this process."
            );
        }
        libc::ENFILE => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': Too many open files on the system."
            );
        }
        libc::ENAMETOOLONG => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': Name is too long."
            );
        }
        libc::EPERM => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': No permission. \
                Either suid/sgid is forbidden or you lack capabilities."
            );
        }

        #[cfg(target_os = "macos")]
        libc::EBADARCH => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "': Bad CPU type in executable."
            );
        }

        err => {
            FLOG_SAFE!(
                exec,
                "Failed to execute process '",
                actual_cmd,
                "', unknown error number ",
                err,
            );
        }
    }
}

/// Returns the interpreter for the specified script. Returns None if file is not a script with a
/// shebang.
fn get_interpreter<'a>(command: &CStr, buffer: &'a mut [u8]) -> Option<&'a CStr> {
    // OK to not use CLO_EXEC here because this is only called after fork.
    let fd = unsafe { libc::open(command.as_ptr(), libc::O_RDONLY) };
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

    #[allow(clippy::if_same_then_else)]
    let offset = if buffer.starts_with(b"#! /") {
        3
    } else if buffer.starts_with(b"#!/") {
        2
    } else if buffer.starts_with(b"#!") {
        // Relative path, basically always an issue.
        2
    } else {
        return None;
    };
    Some(CStr::from_bytes_with_nul(&buffer[offset..idx.max(offset)]).unwrap())
}

/// Set up redirections and signal handling in the child process.
mod ffi {
    use super::*;
    #[no_mangle]
    pub extern "C" fn child_setup_process(
        claim_tty_from: pid_t,
        sigmask: *const libc::sigset_t,
        is_forked: bool,
        dup2s: *const Dup2List,
    ) -> i32 {
        let sigmask = unsafe { sigmask.as_ref() };
        let dup2s = unsafe { &*dup2s };
        super::child_setup_process(claim_tty_from, sigmask, is_forked, dup2s)
    }

    #[no_mangle]
    pub extern "C" fn safe_report_exec_error(
        err: i32,
        actual_cmd: *const c_char,
        argvv: *const *const c_char,
        envv: *const *const c_char,
    ) {
        super::safe_report_exec_error(err, actual_cmd, argvv, envv)
    }

    #[no_mangle]
    pub extern "C" fn execute_fork() -> pid_t {
        super::execute_fork()
    }

    #[no_mangle]
    pub extern "C" fn execute_setpgid(pid: pid_t, pgroup: pid_t, is_parent: bool) -> i32 {
        super::execute_setpgid(pid, pgroup, is_parent)
    }

    #[no_mangle]
    pub extern "C" fn report_setpgid_error(
        err: i32,
        is_parent: bool,
        pid: pid_t,
        desired_pgid: pid_t,
        job_id: c_int,
        command_str: *const c_char,
        argv0_str: *const c_char,
    ) {
        super::report_setpgid_error(
            err,
            is_parent,
            pid,
            desired_pgid,
            job_id,
            command_str,
            argv0_str,
        )
    }
}
