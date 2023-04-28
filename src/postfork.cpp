#if 0
// Functions that we may safely call after fork().
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <cstring>
#ifdef HAVE_SPAWN_H
#include <spawn.h>
#endif
#include <string>
#include <vector>

#include "common.h"
#include "exec.h"
#include "fds.h"
#include "flog.h"
#include "iothread.h"
#include "job_group.rs.h"
#include "postfork.h"
#include "proc.h"
#include "redirection.h"
#include "signals.h"
#include "wutil.h"  // IWYU pragma: keep

#ifndef JOIN_THREADS_BEFORE_FORK
#define JOIN_THREADS_BEFORE_FORK 0
#endif

/// The number of times to try to call fork() before giving up.
#define FORK_LAPS 5

/// The number of nanoseconds to sleep between attempts to call fork().
#define FORK_SLEEP_TIME 1000000

extern bool is_thompson_shell_script(const char *path);
static char *get_interpreter(const char *command, char *buffer, size_t buff_size);

/// Report the error code \p err for a failed setpgid call.
void report_setpgid_error(int err, bool is_parent, pid_t desired_pgid, const job_t *j,
                          const process_t *p) {
    char pid_buff[128];
    char job_id_buff[128];
    char getpgid_buff[128];
    char job_pgid_buff[128];
    char argv0[64];
    char command[64];

    format_long_safe(pid_buff, p->pid);
    format_long_safe(job_id_buff, j->job_id());
    format_long_safe(getpgid_buff, getpgid(p->pid));
    format_long_safe(job_pgid_buff, desired_pgid);
    narrow_string_safe(argv0, p->argv0());
    narrow_string_safe(command, j->command_wcstr());

    FLOGF_SAFE(warning, "Could not send %s %s, '%s' in job %s, '%s' from group %s to group %s",
               is_parent ? "child" : "self", pid_buff, argv0, job_id_buff, command, getpgid_buff,
               job_pgid_buff);

    if (is_windows_subsystem_for_linux() && errno == EPERM) {
        FLOGF_SAFE(warning,
                   "Please update to Windows 10 1809/17763 or higher to address known issues "
                   "with process groups and zombie processes.");
    }

    errno = err;
    switch (errno) {
        case EACCES: {
            FLOGF_SAFE(error, "setpgid: Process %s has already exec'd", pid_buff);
            break;
        }
        case EINVAL: {
            FLOGF_SAFE(error, "setpgid: pgid %s unsupported", getpgid_buff);
            break;
        }
        case EPERM: {
            FLOGF_SAFE(error, "setpgid: Process %s is a session leader or pgid %s does not match",
                       pid_buff, getpgid_buff);
            break;
        }
        case ESRCH: {
            FLOGF_SAFE(error, "setpgid: Process ID %s does not match", pid_buff);
            break;
        }
        default: {
            char errno_buff[64];
            format_long_safe(errno_buff, errno);
            FLOGF_SAFE(error, "setpgid: Unknown error number %s", errno_buff);
            break;
        }
    }
}

int execute_setpgid(pid_t pid, pid_t pgroup, bool is_parent) {
    // Historically we have looped here to support WSL.
    unsigned eperm_count = 0;
    for (;;) {
        if (setpgid(pid, pgroup) == 0) {
            return 0;
        }
        int err = errno;
        if (err == EACCES && is_parent) {
            // We are the parent process and our child has called exec().
            // This is an unavoidable benign race.
            return 0;
        } else if (err == EINTR) {
            // Paranoia.
            continue;
        } else if (err == EPERM && eperm_count++ < 100) {
            // The setpgid(2) man page says that EPERM is returned only if attempts are made
            // to move processes into groups across session boundaries (which can never be
            // the case in fish, anywhere) or to change the process group ID of a session
            // leader (again, can never be the case). I'm pretty sure this is a WSL bug, as
            // we see the same with tcsetpgrp(2) in other places and it disappears on retry.
            FLOGF_SAFE(proc_pgroup, "setpgid(2) returned EPERM. Retrying");
            continue;
        }
#if defined(__BSD__) || defined(__APPLE__)
        // POSIX.1 doesn't specify that zombie processes are required to be considered extant and/or
        // children of the parent for purposes of setpgid(2). In particular, FreeBSD (at least up to
        // 12.2) does not consider a child that has already forked, exec'd, and exited to "exist"
        // and returns ESRCH (process not found) instead of EACCES (child has called exec).
        // See https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=251227
        else if (err == ESRCH && is_parent) {
            // Handle this just like we would EACCES above, as we're virtually certain that
            // setpgid(2) was called against a process that was at least at one point in time a
            // valid child.
            return 0;
        }
#endif

        return err;
    }
}

int child_setup_process(pid_t claim_tty_from, const job_t &job, bool is_forked,
                        const dup2_list_t &dup2s) {
    // Note we are called in a forked child.
    for (const auto &act : dup2s.get_actions()) {
        int err;
        if (act.target < 0) {
            err = close(act.src);
        } else if (act.target != act.src) {
            // Normal redirection.
            err = dup2(act.src, act.target);
        } else {
            // This is a weird case like /bin/cmd 6< file.txt
            // The opened file (which is CLO_EXEC) wants to be dup2'd to its own fd.
            // We need to unset the CLO_EXEC flag.
            err = set_cloexec(act.src, false);
        }
        if (err < 0) {
            if (is_forked) {
                FLOGF_SAFE(warning, "failed to set up file descriptors in child_setup_process");
                exit_without_destructors(1);
            }
            return err;
        }
    }
    if (claim_tty_from >= 0 && tcgetpgrp(STDIN_FILENO) == claim_tty_from) {
        // Assign the terminal within the child to avoid the well-known race between tcsetgrp() in
        // the parent and the child executing. We are not interested in error handling here, except
        // we try to avoid this for non-terminals; in particular pipelines often make non-terminal
        // stdin.
        // Only do this if the tty currently belongs to fish's pgrp. Don't try to steal it away from
        // another process which may happen if we are run in the background with job control
        // enabled. Note if stdin is not a tty, then tcgetpgrp() will return -1 and we will not
        // enter this.
        // Ensure this doesn't send us to the background (see #5963)
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        (void)tcsetpgrp(STDIN_FILENO, getpid());
    }
    sigset_t sigmask;
    sigemptyset(&sigmask);
    if (blocked_signals_for_job(job, &sigmask)) {
        sigprocmask(SIG_SETMASK, &sigmask, nullptr);
    }
    // Set the handling for job control signals back to the default.
    // Do this after any tcsetpgrp call so that we swallow SIGTTIN.
    signal_reset_handlers();
    return 0;
}

/// This function is a wrapper around fork. If the fork calls fails with EAGAIN, it is retried
/// FORK_LAPS times, with a very slight delay between each lap. If fork fails even then, the process
/// will exit with an error message.
pid_t execute_fork() {
    if (JOIN_THREADS_BEFORE_FORK) {
        // Make sure we have no outstanding threads before we fork. This is a pretty sketchy thing
        // to do here, both because exec.cpp shouldn't have to know about iothreads, and because the
        // completion handlers may do unexpected things.
        FLOGF_SAFE(iothread, "waiting for threads to drain.");
        iothread_drain_all();
    }

    pid_t pid;
    struct timespec pollint;
    int i;

    for (i = 0; i < FORK_LAPS; i++) {
        pid = fork();
        if (pid >= 0) {
            return pid;
        }

        if (errno != EAGAIN) {
            break;
        }

        pollint.tv_sec = 0;
        pollint.tv_nsec = FORK_SLEEP_TIME;

        // Don't sleep on the final lap - sleeping might change the value of errno, which will break
        // the error reporting below.
        if (i != FORK_LAPS - 1) {
            nanosleep(&pollint, nullptr);
        }
    }

    // These are all the errno numbers for fork() I can find.
    // Also ENOSYS, but I doubt anyone is running
    // fish on a platform without an MMU.
    switch (errno) {
        case EAGAIN: {
            // We should have retried these already?
            FLOGF_SAFE(error, "fork: Out of resources. Check RLIMIT_NPROC and pid_max.");
            break;
        }
        case ENOMEM: {
            FLOGF_SAFE(error, "fork: Out of memory.");
            break;
        }
        default: {
            char errno_buff[64];
            format_long_safe(errno_buff, errno);
            FLOGF_SAFE(error, "fork: Unknown error number %s", errno_buff);
            break;
        }
    }
    FATAL_EXIT();
    return 0;
}

#if FISH_USE_POSIX_SPAWN

// Given an error code, if it is the first error, record it.
// \return whether we have any error.
bool posix_spawner_t::check_fail(int err) {
    if (error_ == 0) error_ = err;
    return error_ != 0;
}

posix_spawner_t::~posix_spawner_t() {
    if (attr_.has_value()) {
        posix_spawnattr_destroy(this->attr());
    }
    if (actions_.has_value()) {
        posix_spawn_file_actions_destroy(this->actions());
    }
}

posix_spawner_t::posix_spawner_t(const job_t *j, const dup2_list_t &dup2s) {
    // Initialize our fields. This may fail.
    {
        posix_spawnattr_t attr;
        if (check_fail(posix_spawnattr_init(&attr))) return;
        this->attr_ = attr;
    }

    {
        posix_spawn_file_actions_t actions;
        if (check_fail(posix_spawn_file_actions_init(&actions))) return;
        this->actions_ = actions;
    }

    // desired_pgid tracks the pgroup for the process. If it is none, the pgroup is left unchanged.
    // If it is zero, create a new pgroup from the pid. If it is >0, join that pgroup.
    maybe_t<pid_t> desired_pgid = none();
    {
        auto pgid = j->group->get_pgid();
        if (pgid) {
            desired_pgid = pgid->value;
        } else if (j->processes.front()->leads_pgrp) {
            desired_pgid = 0;
        }
    }

    // Set the handling for job control signals back to the default.
    bool reset_signal_handlers = true;

    // Remove all signal blocks.
    bool reset_sigmask = true;

    // Set our flags.
    short flags = 0;
    if (reset_signal_handlers) flags |= POSIX_SPAWN_SETSIGDEF;
    if (reset_sigmask) flags |= POSIX_SPAWN_SETSIGMASK;
    if (desired_pgid.has_value()) flags |= POSIX_SPAWN_SETPGROUP;

    if (check_fail(posix_spawnattr_setflags(attr(), flags))) return;

    if (desired_pgid.has_value()) {
        if (check_fail(posix_spawnattr_setpgroup(attr(), *desired_pgid))) return;
    }

    // Everybody gets default handlers.
    if (reset_signal_handlers) {
        sigset_t sigdefault;
        get_signals_with_handlers(&sigdefault);
        if (check_fail(posix_spawnattr_setsigdefault(attr(), &sigdefault))) return;
    }

    // No signals blocked.
    if (reset_sigmask) {
        sigset_t sigmask;
        sigemptyset(&sigmask);
        blocked_signals_for_job(*j, &sigmask);
        if (check_fail(posix_spawnattr_setsigmask(attr(), &sigmask))) return;
    }

    // Apply our dup2s.
    for (const auto &act : dup2s.get_actions()) {
        if (act.target < 0) {
            if (check_fail(posix_spawn_file_actions_addclose(actions(), act.src))) return;
        } else {
            if (check_fail(posix_spawn_file_actions_adddup2(actions(), act.src, act.target)))
                return;
        }
    }
}

maybe_t<pid_t> posix_spawner_t::spawn(const char *cmd, char *const argv[], char *const envp[]) {
    if (get_error()) return none();
    pid_t pid = -1;
    if (check_fail(posix_spawn(&pid, cmd, &*actions_, &*attr_, argv, envp))) {
        // The shebang wasn't introduced until UNIX Seventh Edition, so if
        // the kernel won't run the binary we hand it off to the interpreter
        // after performing a binary safety check, recommended by POSIX: a
        // line needs to exist before the first \0 with a lowercase letter
        if (error_ == ENOEXEC && is_thompson_shell_script(cmd)) {
            error_ = 0;
            // Create a new argv with /bin/sh prepended.
            std::vector<char *> argv2;
            char interp[] = _PATH_BSHELL;
            argv2.push_back(interp);
            // The command to call should use the full path,
            // not what we would pass as argv0.
            std::string cmd2 = cmd;
            argv2.push_back(&cmd2[0]);
            for (size_t i = 1; argv[i] != nullptr; i++) {
                argv2.push_back(argv[i]);
            }
            argv2.push_back(nullptr);
            if (check_fail(posix_spawn(&pid, interp, &*actions_, &*attr_, &argv2[0], envp))) {
                return none();
            }
        } else {
            return none();
        }
    }
    return pid;
}

#endif  // FISH_USE_POSIX_SPAWN

void safe_report_exec_error(int err, const char *actual_cmd, const char *const *argv,
                            const char *const *envv) {
    switch (err) {
        case E2BIG: {
            char sz1[128];

            long arg_max = -1;

            size_t sz = 0;
            size_t szenv = 0;
            const char *const *p;
            for (p = argv; *p; p++) {
                sz += std::strlen(*p) + 1;
            }

            for (p = envv; *p; p++) {
                szenv += std::strlen(*p) + 1;
            }
            sz += szenv;

            format_size_safe(sz1, sz);
            arg_max = sysconf(_SC_ARG_MAX);

            if (arg_max > 0) {
                if (sz >= static_cast<unsigned long long>(arg_max)) {
                    char sz2[128];
                    format_size_safe(sz2, static_cast<unsigned long long>(arg_max));
                    FLOGF_SAFE(
                        exec,
                        "Failed to execute process '%s': the total size of the argument list and "
                        "exported variables (%s) exceeds the OS limit of %s.",
                        actual_cmd, sz1, sz2);
                } else {
                    // MAX_ARG_STRLEN, a linux thing that limits the size of one argument. It's
                    // defined in binfmts.h, but we don't want to include that just to be able to
                    // print the real limit.
                    FLOGF_SAFE(exec,
                               "Failed to execute process '%s': An argument or exported variable "
                               "exceeds the OS "
                               "argument length limit.",
                               actual_cmd);
                }

                if (szenv >= static_cast<unsigned long long>(arg_max) / 2) {
                    FLOGF_SAFE(exec,
                               "Hint: Your exported variables take up over half the limit. Try "
                               "erasing or unexporting variables.");
                }
            } else {
                FLOGF_SAFE(
                    exec,
                    "Failed to execute process '%s': the total size of the argument list and "
                    "exported variables (%s) exceeds the "
                    "operating system limit.",
                    actual_cmd, sz1);
            }
            break;
        }

        case ENOEXEC: {
            FLOGF_SAFE(exec,
                       "Failed to execute process: '%s' the file could not be run by the "
                       "operating system.",
                       actual_cmd);
            char interpreter_buff[128] = {};
            const char *interpreter =
                get_interpreter(actual_cmd, interpreter_buff, sizeof interpreter_buff);
            if (!interpreter) {
                // Paths ending in ".fish" need to start with a shebang
                if (const char *lastdot = strrchr(actual_cmd, '.')) {
                    if (0 == strcmp(lastdot, ".fish")) {
                        FLOGF_SAFE(exec,
                                   "fish scripts require an interpreter directive (must start with "
                                   "'#!/path/to/fish').");
                    }
                }
            } else {
                // If the shebang line exists, we would get an ENOENT or similar instead,
                // so I don't know how to reach this.
                FLOGF_SAFE(exec, "Maybe the interpreter directive (#! line) is broken?",
                           actual_cmd);
            }
            break;
        }

        case EACCES:
        case ENOENT: {
            // ENOENT is returned by exec() when the path fails, but also returned by posix_spawn if
            // an open file action fails. These cases appear to be impossible to distinguish. We
            // address this by not using posix_spawn for file redirections, so all the ENOENTs we
            // find must be errors from exec().
            char interpreter_buff[128] = {};
            const char *interpreter =
                get_interpreter(actual_cmd, interpreter_buff, sizeof interpreter_buff);
            struct stat buf;
            auto statret = stat(interpreter, &buf);
            if (interpreter && (0 != statret || access(interpreter, X_OK))) {
                // Detect Windows line endings and complain specifically about them.
                auto len = strlen(interpreter);
                if (len && interpreter[len - 1] == '\r') {
                    FLOGF_SAFE(exec,
                               "Failed to execute process '%s':  The file uses Windows line "
                               "endings (\\r\\n). Run dos2unix or similar to fix it.",
                               actual_cmd);
                } else {
                    FLOGF_SAFE(exec,
                               "Failed to execute process '%s': The file specified the interpreter "
                               "'%s', which is not an "
                               "executable command.",
                               actual_cmd, interpreter);
                }
            } else if (interpreter && S_ISDIR(buf.st_mode)) {
                FLOGF_SAFE(exec,
                           "Failed to execute process '%s': The file specified the interpreter "
                           "'%s', which is a directory.",
                           actual_cmd, interpreter);
            } else if (access(actual_cmd, X_OK) == 0) {
                FLOGF_SAFE(exec,
                           "Failed to execute process '%s': The file exists and is executable. "
                           "Check the interpreter or linker?",
                           actual_cmd);
            } else if (err == ENOENT) {
                FLOGF_SAFE(exec,
                           "Failed to execute process '%s': The file does not exist or could not "
                           "be executed.",
                           actual_cmd);
            } else {
                FLOGF_SAFE(exec, "Failed to execute process '%s': The file could not be accessed.",
                           actual_cmd);
            }
            break;
        }
        case ENOMEM: {
            FLOGF_SAFE(exec, "Out of memory");
            break;
        }
        case ETXTBSY: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': File is currently open for writing.",
                       actual_cmd);
            break;
        }
        case ELOOP: {
            FLOGF_SAFE(
                exec,
                "Failed to execute process '%s': Too many layers of symbolic links. Maybe a loop?",
                actual_cmd);
            break;
        }
        case EINVAL: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': Unsupported format.", actual_cmd);
            break;
        }
        case EISDIR: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': File is a directory.", actual_cmd);
            break;
        }
        case ENOTDIR: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': A path component is not a directory.",
                       actual_cmd);
            break;
        }

        case EMFILE: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': Too many open files in this process.",
                       actual_cmd);
            break;
        }
        case ENFILE: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': Too many open files on the system.",
                       actual_cmd);
            break;
        }
        case ENAMETOOLONG: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': Name is too long.", actual_cmd);
            break;
        }
        case EPERM: {
            FLOGF_SAFE(exec,
                       "Failed to execute process '%s': No permission. Either suid/sgid is "
                       "forbidden or you lack capabilities.",
                       actual_cmd);
            break;
        }
#ifdef EBADARCH
        case EBADARCH: {
            FLOGF_SAFE(exec, "Failed to execute process '%s': Bad CPU type in executable.",
                       actual_cmd);
            break;
        }
#endif
        default: {
            char errnum_buff[64];
            format_long_safe(errnum_buff, err);
            FLOGF_SAFE(exec, "Failed to execute process '%s', unknown error number %s", actual_cmd,
                       errnum_buff);
            break;
        }
    }
}

/// Returns the interpreter for the specified script. Returns NULL if file is not a script with a
/// shebang.
static char *get_interpreter(const char *command, char *buffer, size_t buff_size) {
    // OK to not use CLO_EXEC here because this is only called after fork.
    int fd = open(command, O_RDONLY);
    if (fd >= 0) {
        size_t idx = 0;
        while (idx + 1 < buff_size) {
            char ch;
            ssize_t amt = read(fd, &ch, sizeof ch);
            if (amt <= 0) break;
            if (ch == '\n') break;
            buffer[idx++] = ch;
        }
        buffer[idx++] = '\0';
        close(fd);
    }

    if (std::strncmp(buffer, "#! /", const_strlen("#! /")) == 0) {
        return buffer + 3;
    } else if (std::strncmp(buffer, "#!/", const_strlen("#!/")) == 0) {
        return buffer + 2;
    } else if (std::strncmp(buffer, "#!", const_strlen("#!")) == 0) {
        // Relative path, basically always an issue.
        return buffer + 2;
    }
    return nullptr;
};
#endif
