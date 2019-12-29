// Functions that we may safely call after fork().
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#include <cstring>
#include <memory>
#if FISH_USE_POSIX_SPAWN
#include <spawn.h>
#endif
#include <cwchar>

#include "common.h"
#include "exec.h"
#include "flog.h"
#include "io.h"
#include "iothread.h"
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

/// Called only by the child to set its own process group (possibly creating a new group in the
/// process if it is the first in a JOB_CONTROL job.
/// Returns true on success, false on failure.
bool child_set_group(job_t *j, process_t *p) {
    if (j->wants_job_control()) {
        if (j->pgid == INVALID_PID) {
            j->pgid = p->pid;
        }

        for (int i = 0; setpgid(p->pid, j->pgid) != 0; ++i) {
            // Put a cap on how many times we retry so we are never stuck here
            if (i < 100) {
                if (errno == EPERM) {
                    // The setpgid(2) man page says that EPERM is returned only if attempts are made
                    // to move processes into groups across session boundaries (which can never be
                    // the case in fish, anywhere) or to change the process group ID of a session
                    // leader (again, can never be the case). I'm pretty sure this is a WSL bug, as
                    // we see the same with tcsetpgrp(2) in other places and it disappears on retry.
                    debug_safe(2, "setpgid(2) returned EPERM. Retrying");
                    continue;
                } else if (errno == EINTR) {
                    // I don't think signals are blocked here. The parent (fish) redirected the
                    // signal handlers and `child_setup_process()` calls `signal_reset_handlers()`
                    // after we're done here (and not `signal_unblock()`). We're already in a loop,
                    // so let's just handle EINTR just in case.
                    continue;
                }
            }

            char pid_buff[128];
            char job_id_buff[128];
            char getpgid_buff[128];
            char job_pgid_buff[128];
            char argv0[64];
            char command[64];

            format_long_safe(pid_buff, p->pid);
            format_long_safe(job_id_buff, j->job_id());
            format_long_safe(getpgid_buff, getpgid(p->pid));
            format_long_safe(job_pgid_buff, j->pgid);
            narrow_string_safe(argv0, p->argv0());
            narrow_string_safe(command, j->command_wcstr());

            debug_safe(
                1, "Could not send own process %s, '%s' in job %s, '%s' from group %s to group %s",
                pid_buff, argv0, job_id_buff, command, getpgid_buff, job_pgid_buff);

            if (is_windows_subsystem_for_linux() && errno == EPERM) {
                debug_safe(
                    1,
                    "Please update to Windows 10 1809/17763 or higher to address known issues "
                    "with process groups and zombie processes.");
            }

            safe_perror("setpgid");

            return false;
        }
    } else {
        j->pgid = getpgrp();
    }
    return true;
}

/// Called only by the parent only after a child forks and successfully calls child_set_group,
/// guaranteeing the job control process group has been created and that the child belongs to the
/// correct process group. Here we can update our job_t structure to reflect the correct process
/// group in the case of JOB_CONTROL, and we can give the new process group control of the terminal
/// if it's to run in the foreground.
bool set_child_group(job_t *j, pid_t child_pid) {
    if (j->wants_job_control()) {
        assert(j->pgid != INVALID_PID &&
               "set_child_group called with JOB_CONTROL before job pgid determined!");

        // The parent sets the child's group. This incurs the well-known unavoidable race with the
        // child exiting, so ignore ESRCH and EPERM (in case the pid was recycled).
        // Additionally ignoring EACCES. See #4715 and #4884.
        if (setpgid(child_pid, j->pgid) < 0) {
            if (errno != ESRCH && errno != EPERM && errno != EACCES) {
                safe_perror("setpgid");
                return false;
            } else {
                // Just in case it's ever not right to ignore the setpgid call, (i.e. if this
                // ever leads to a terminal hang due if both this setpgid call AND posix_spawn's
                // internal setpgid calls failed), write to the debug log so a future developer
                // doesn't go crazy trying to track this down.
                debug(2, "Error %d while calling setpgid for child %d (probably harmless)", errno,
                      child_pid);
            }
        }
    } else {
        j->pgid = getpgrp();
    }

    return true;
}

int child_setup_process(pid_t new_termowner, bool is_forked, const dup2_list_t &dup2s) {
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
    if (new_termowner != INVALID_PID) {
        // Assign the terminal within the child to avoid the well-known race between tcsetgrp() in
        // the parent and the child executing. We are not interested in error handling here, except
        // we try to avoid this for non-terminals; in particular pipelines often make non-terminal
        // stdin.
        if (isatty(STDIN_FILENO)) {
            // Ensure this doesn't send us to the background (see #5963)
            signal(SIGTTIN, SIG_IGN);
            signal(SIGTTOU, SIG_IGN);
            (void)tcsetpgrp(STDIN_FILENO, new_termowner);
        }
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
bool fork_actions_make_spawn_properties(posix_spawnattr_t *attr,
                                        posix_spawn_file_actions_t *actions, const job_t *j,
                                        const dup2_list_t &dup2s) {
    // Initialize the output.
    if (posix_spawnattr_init(attr) != 0) {
        return false;
    }

    if (posix_spawn_file_actions_init(actions) != 0) {
        posix_spawnattr_destroy(attr);
        return false;
    }

    bool should_set_process_group_id = false;
    int desired_process_group_id = 0;
    if (j->wants_job_control()) {
        should_set_process_group_id = true;

        // set_child_group puts each job into its own process group
        // do the same here if there is no PGID yet (i.e. PGID == -2)
        desired_process_group_id = j->pgid;
        if (desired_process_group_id == INVALID_PID) {
            desired_process_group_id = 0;
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
    if (should_set_process_group_id) flags |= POSIX_SPAWN_SETPGROUP;

    int err = 0;
    if (!err) err = posix_spawnattr_setflags(attr, flags);

    if (!err && should_set_process_group_id)
        err = posix_spawnattr_setpgroup(attr, desired_process_group_id);

    // Everybody gets default handlers.
    if (!err && reset_signal_handlers) {
        sigset_t sigdefault;
        get_signals_with_handlers(&sigdefault);
        err = posix_spawnattr_setsigdefault(attr, &sigdefault);
    }

    // No signals blocked.
    sigset_t sigmask;
    sigemptyset(&sigmask);
    if (!err && reset_sigmask) err = posix_spawnattr_setsigmask(attr, &sigmask);

    // Apply our dup2s.
    for (const auto &act : dup2s.get_actions()) {
        if (err) break;
        if (act.target < 0) {
            err = posix_spawn_file_actions_addclose(actions, act.src);
        } else {
            err = posix_spawn_file_actions_adddup2(actions, act.src, act.target);
        }
    }

    // Clean up on error.
    if (err) {
        posix_spawnattr_destroy(attr);
        posix_spawn_file_actions_destroy(actions);
    }

    return !err;
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
                format_size_safe(sz2, static_cast<unsigned long long>(arg_max));
                debug_safe(0,
                           "The total size of the argument and environment lists %s exceeds the "
                           "operating system limit of %s.",
                           sz1, sz2);
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
            char interpreter_buff[128] = {}, *interpreter;
            interpreter = get_interpreter(actual_cmd, interpreter_buff, sizeof interpreter_buff);
            if (interpreter && 0 != access(interpreter, X_OK)) {
                debug_safe(0,
                           "The file '%s' specified the interpreter '%s', which is not an "
                           "executable command.",
                           actual_cmd, interpreter);
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

            // FLOGF(error, L"The file '%ls' is marked as an executable but could not be run by the
            // operating system.", p->actual_cmd);
            break;
        }
    }
}
