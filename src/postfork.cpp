// Functions that we may safely call after fork().
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#include <cstring>
#include <memory>
#ifdef FISH_USE_POSIX_SPAWN
#include <spawn.h>
#endif
#include <cwchar>

#include "common.h"
#include "exec.h"
#include "flog.h"
#include "io.h"
#include "iothread.h"
#include "job_group.h"
#include "postfork.h"
#include "proc.h"
#include "redirection.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

#ifndef JOIN_THREADS_BEFORE_FORK
#define JOIN_THREADS_BEFORE_FORK 0
#endif

/// The number of times to try to call fork() before giving up.
#define FORK_LAPS 5

/// The number of nanoseconds to sleep between attempts to call fork().
#define FORK_SLEEP_TIME 1000000

/// Fork error message.
#define FORK_ERROR "Could not create child process - exiting"

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

    debug_safe(1, "Could not send %s %s, '%s' in job %s, '%s' from group %s to group %s",
               is_parent ? "child" : "self", pid_buff, argv0, job_id_buff, command, getpgid_buff,
               job_pgid_buff);

    if (is_windows_subsystem_for_linux() && errno == EPERM) {
        debug_safe(1,
                   "Please update to Windows 10 1809/17763 or higher to address known issues "
                   "with process groups and zombie processes.");
    }

    errno = err;
    safe_perror("setpgid");
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
            debug_safe(2, "setpgid(2) returned EPERM. Retrying");
            continue;
        }
#ifdef __BSD__
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

int child_setup_process(pid_t new_termowner, pid_t fish_pgrp, const job_t &job, bool is_forked,
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
                debug_safe(4, "redirect_in_child_after_fork failed in child_setup_process");
                exit_without_destructors(1);
            }
            return err;
        }
    }
    if (new_termowner != INVALID_PID && new_termowner != fish_pgrp) {
        // Assign the terminal within the child to avoid the well-known race between tcsetgrp() in
        // the parent and the child executing. We are not interested in error handling here, except
        // we try to avoid this for non-terminals; in particular pipelines often make non-terminal
        // stdin.
        // Only do this if the tty currently belongs to fish's pgrp. Don't try to steal it away from
        // another process which may happen if we are run in the background with job control
        // enabled. Note if stdin is not a tty, then tcgetpgrp() will return -1 and we will not
        // enter this.
        if (tcgetpgrp(STDIN_FILENO) == fish_pgrp) {
            // Ensure this doesn't send us to the background (see #5963)
            signal(SIGTTIN, SIG_IGN);
            signal(SIGTTOU, SIG_IGN);
            (void)tcsetpgrp(STDIN_FILENO, new_termowner);
        }
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
    ASSERT_IS_MAIN_THREAD();

    if (JOIN_THREADS_BEFORE_FORK) {
        // Make sure we have no outstanding threads before we fork. This is a pretty sketchy thing
        // to do here, both because exec.cpp shouldn't have to know about iothreads, and because the
        // completion handlers may do unexpected things.
        debug_safe(4, "waiting for threads to drain.");
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

    debug_safe(0, FORK_ERROR);
    safe_perror("fork");
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
    if (attr_) {
        posix_spawnattr_destroy(this->attr());
    }
    if (actions_) {
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
    if (auto job_pgid = j->group->get_pgid()) {
        desired_pgid = *job_pgid;
    } else {
        assert(j->group->needs_pgid_assignment() && "We should be expecting a pgid");
        // We are the first external proc in the job group. Set the desired_pgid to 0 to indicate we
        // should creating a new process group.
        desired_pgid = 0;
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
    if (check_fail(posix_spawn(&pid, cmd, &*actions_, &*attr_, argv, envp))) return none();
    return pid;
}

#endif  // FISH_USE_POSIX_SPAWN

void safe_report_exec_error(int err, const char *actual_cmd, const char *const *argv,
                            const char *const *envv) {
    debug_safe(0, "Failed to execute process '%s'. Reason:", actual_cmd);

    switch (err) {
        case E2BIG: {
            char sz1[128], sz2[128];

            long arg_max = -1;

            size_t sz = 0;
            const char *const *p;
            for (p = argv; *p; p++) {
                sz += std::strlen(*p) + 1;
            }

            for (p = envv; *p; p++) {
                sz += std::strlen(*p) + 1;
            }

            format_size_safe(sz1, sz);
            arg_max = sysconf(_SC_ARG_MAX);

            if (arg_max > 0) {
                if (sz >= static_cast<unsigned long long>(arg_max)) {
                    format_size_safe(sz2, static_cast<unsigned long long>(arg_max));
                    debug_safe(
                        0,
                        "The total size of the argument and environment lists %s exceeds the "
                        "operating system limit of %s.",
                        sz1, sz2);
                } else {
                    // MAX_ARG_STRLEN, a linux thing that limits the size of one argument. It's
                    // defined in binfmts.h, but we don't want to include that just to be able to
                    // print the real limit.
                    debug_safe(0,
                               "One of your arguments exceeds the operating system's argument "
                               "length limit.");
                }
            } else {
                debug_safe(0,
                           "The total size of the argument and environment lists (%s) exceeds the "
                           "operating system limit.",
                           sz1);
            }

            debug_safe(0, "Try running the command again with fewer arguments.");
            break;
        }

        case ENOEXEC: {
            const char *err = safe_strerror(errno);
            debug_safe(0, "exec: %s", err);

            debug_safe(0,
                       "The file '%s' is marked as an executable but could not be run by the "
                       "operating system.",
                       actual_cmd);
            break;
        }

        case ENOENT: {
            // ENOENT is returned by exec() when the path fails, but also returned by posix_spawn if
            // an open file action fails. These cases appear to be impossible to distinguish. We
            // address this by not using posix_spawn for file redirections, so all the ENOENTs we
            // find must be errors from exec().
            char interpreter_buff[128] = {};
            const char *interpreter =
                get_interpreter(actual_cmd, interpreter_buff, sizeof interpreter_buff);
            if (interpreter && 0 != access(interpreter, X_OK)) {
                // Detect windows line endings and complain specifically about them.
                auto len = strlen(interpreter);
                if (len && interpreter[len - 1] == '\r') {
                    debug_safe(0,
                               "The file uses windows line endings (\\r\\n). Run dos2unix or similar to fix it.");
                } else {
                    debug_safe(0,
                               "The file '%s' specified the interpreter '%s', which is not an "
                               "executable command.",
                               actual_cmd, interpreter);
                }
            } else {
                debug_safe(0, "The file '%s' does not exist or could not be executed.", actual_cmd);
            }
            break;
        }

        case ENOMEM: {
            debug_safe(0, "Out of memory");
            break;
        }

        default: {
            const char *err = safe_strerror(errno);
            debug_safe(0, "exec: %s", err);
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

    if (std::strncmp(buffer, "#! /", 4) == 0) {
        return buffer + 3;
    } else if (std::strncmp(buffer, "#!/", 3) == 0) {
        return buffer + 2;
    }
    return nullptr;
};
