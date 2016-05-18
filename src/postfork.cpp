// Functions that we may safely call after fork().
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <memory>
#if FISH_USE_POSIX_SPAWN
#include <spawn.h>
#endif

#include "common.h"
#include "exec.h"
#include "io.h"
#include "iothread.h"
#include "postfork.h"
#include "proc.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

#ifndef JOIN_THREADS_BEFORE_FORK
#define JOIN_THREADS_BEFORE_FORK 0
#endif

/// The number of times to try to call fork() before giving up.
#define FORK_LAPS 5

/// The number of nanoseconds to sleep between attempts to call fork().
#define FORK_SLEEP_TIME 1000000

/// Base open mode to pass to calls to open.
#define OPEN_MASK 0666

/// Fork error message.
#define FORK_ERROR "Could not create child process - exiting"

/// File redirection clobbering error message.
#define NOCLOB_ERROR "The file '%s' already exists"

/// File redirection error message.
#define FILE_ERROR "An error occurred while redirecting file '%s'"

/// File descriptor redirection error message.
#define FD_ERROR "An error occurred while redirecting file descriptor %s"

/// Pipe error message.
#define LOCAL_PIPE_ERROR "An error occurred while setting up pipe"

static bool log_redirections = false;

/// Cover for debug_safe that can take an int. The format string should expect a %s.
static void debug_safe_int(int level, const char *format, int val) {
    char buff[128];
    format_long_safe(buff, val);
    debug_safe(level, format, buff);
}

int set_child_group(job_t *j, process_t *p, int print_errors) {
    int res = 0;

    if (job_get_flag(j, JOB_CONTROL)) {
        if (!j->pgid) {
            j->pgid = p->pid;
        }

        if (setpgid(p->pid, j->pgid)) {
            if (getpgid(p->pid) != j->pgid && print_errors) {
                char pid_buff[128];
                char job_id_buff[128];
                char getpgid_buff[128];
                char job_pgid_buff[128];
                char argv0[64];
                char command[64];

                format_long_safe(pid_buff, p->pid);
                format_long_safe(job_id_buff, j->job_id);
                format_long_safe(getpgid_buff, getpgid(p->pid));
                format_long_safe(job_pgid_buff, j->pgid);
                narrow_string_safe(argv0, p->argv0());
                narrow_string_safe(command, j->command_wcstr());

                debug_safe(
                    1, "Could not send process %s, '%s' in job %s, '%s' from group %s to group %s",
                    pid_buff, argv0, job_id_buff, command, getpgid_buff, job_pgid_buff);

                safe_perror("setpgid");
                res = -1;
            }
        }
    } else {
        j->pgid = getpid();
    }

    if (job_get_flag(j, JOB_TERMINAL) && job_get_flag(j, JOB_FOREGROUND)) {
        if (tcsetpgrp(0, j->pgid) && print_errors) {
            char job_id_buff[64];
            char command_buff[64];
            format_long_safe(job_id_buff, j->job_id);
            narrow_string_safe(command_buff, j->command_wcstr());
            debug_safe(1, "Could not send job %s ('%s') to foreground", job_id_buff, command_buff);
            safe_perror("tcsetpgrp");
            res = -1;
        }
    }

    return res;
}

/// Set up a childs io redirections. Should only be called by setup_child_process(). Does the
/// following: First it closes any open file descriptors not related to the child by calling
/// close_unused_internal_pipes() and closing the universal variable server file descriptor. It then
/// goes on to perform all the redirections described by \c io.
///
/// \param io the list of IO redirections for the child
///
/// \return 0 on sucess, -1 on failure
static int handle_child_io(const io_chain_t &io_chain) {
    for (size_t idx = 0; idx < io_chain.size(); idx++) {
        const io_data_t *io = io_chain.at(idx).get();

        if (io->io_mode == IO_FD && io->fd == static_cast<const io_fd_t *>(io)->old_fd) {
            continue;
        }

        switch (io->io_mode) {
            case IO_CLOSE: {
                if (log_redirections) fprintf(stderr, "%d: close %d\n", getpid(), io->fd);
                if (close(io->fd)) {
                    debug_safe_int(0, "Failed to close file descriptor %s", io->fd);
                    safe_perror("close");
                }
                break;
            }

            case IO_FILE: {
                // Here we definitely do not want to set CLO_EXEC because our child needs access.
                CAST_INIT(const io_file_t *, io_file, io);
                int tmp = open(io_file->filename_cstr, io_file->flags, OPEN_MASK);
                if (tmp < 0) {
                    if ((io_file->flags & O_EXCL) && (errno == EEXIST)) {
                        debug_safe(1, NOCLOB_ERROR, io_file->filename_cstr);
                    } else {
                        debug_safe(1, FILE_ERROR, io_file->filename_cstr);
                        safe_perror("open");
                    }

                    return -1;
                } else if (tmp != io->fd) {
                    // This call will sometimes fail, but that is ok, this is just a precausion.
                    close(io->fd);

                    if (dup2(tmp, io->fd) == -1) {
                        debug_safe_int(1, FD_ERROR, io->fd);
                        safe_perror("dup2");
                        exec_close(tmp);
                        return -1;
                    }
                    exec_close(tmp);
                }
                break;
            }

            case IO_FD: {
                int old_fd = static_cast<const io_fd_t *>(io)->old_fd;
                if (log_redirections)
                    fprintf(stderr, "%d: fd dup %d to %d\n", getpid(), old_fd, io->fd);

                // This call will sometimes fail, but that is ok, this is just a precausion.
                close(io->fd);

                if (dup2(old_fd, io->fd) == -1) {
                    debug_safe_int(1, FD_ERROR, io->fd);
                    safe_perror("dup2");
                    return -1;
                }
                break;
            }

            case IO_BUFFER:
            case IO_PIPE: {
                CAST_INIT(const io_pipe_t *, io_pipe, io);
                // If write_pipe_idx is 0, it means we're connecting to the read end (first pipe
                // fd). If it's 1, we're connecting to the write end (second pipe fd).
                unsigned int write_pipe_idx = (io_pipe->is_input ? 0 : 1);
#if 0
                debug( 0, L"%ls %ls on fd %d (%d %d)", write_pipe?L"write":L"read",
                        (io->io_mode == IO_BUFFER)?L"buffer":L"pipe", io->fd, io->pipe_fd[0],
                        io->pipe_fd[1]);
#endif
                if (log_redirections)
                    fprintf(stderr, "%d: %s dup %d to %d\n", getpid(),
                            io->io_mode == IO_BUFFER ? "buffer" : "pipe",
                            io_pipe->pipe_fd[write_pipe_idx], io->fd);
                if (dup2(io_pipe->pipe_fd[write_pipe_idx], io->fd) != io->fd) {
                    debug_safe(1, LOCAL_PIPE_ERROR);
                    safe_perror("dup2");
                    return -1;
                }

                if (io_pipe->pipe_fd[0] >= 0) exec_close(io_pipe->pipe_fd[0]);
                if (io_pipe->pipe_fd[1] >= 0) exec_close(io_pipe->pipe_fd[1]);
                break;
            }
        }
    }

    return 0;
}

int setup_child_process(job_t *j, process_t *p, const io_chain_t &io_chain) {
    bool ok = true;

    if (p) {
        ok = (0 == set_child_group(j, p, 1));
    }

    if (ok) {
        ok = (0 == handle_child_io(io_chain));
        if (p != 0 && !ok) {
            exit_without_destructors(1);
        }
    }

    if (ok) {
        // Set the handling for job control signals back to the default.
        signal_reset_handlers();
    }

    signal_unblock();  // remove all signal blocks

    return ok ? 0 : -1;
}

int g_fork_count = 0;

/// This function is a wrapper around fork. If the fork calls fails with EAGAIN, it is retried
/// FORK_LAPS times, with a very slight delay between each lap. If fork fails even then, the process
/// will exit with an error message.
pid_t execute_fork(bool wait_for_threads_to_die) {
    ASSERT_IS_MAIN_THREAD();

    if (wait_for_threads_to_die || JOIN_THREADS_BEFORE_FORK) {
        // Make sure we have no outstanding threads before we fork. This is a pretty sketchy thing
        // to do here, both because exec.cpp shouldn't have to know about iothreads, and because the
        // completion handlers may do unexpected things.
        iothread_drain_all();
    }

    pid_t pid;
    struct timespec pollint;
    int i;

    g_fork_count++;

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
            nanosleep(&pollint, NULL);
        }
    }

    debug_safe(0, FORK_ERROR);
    safe_perror("fork");
    FATAL_EXIT();
    return 0;
}

#if FISH_USE_POSIX_SPAWN
bool fork_actions_make_spawn_properties(posix_spawnattr_t *attr,
                                        posix_spawn_file_actions_t *actions, job_t *j, process_t *p,
                                        const io_chain_t &io_chain) {
    // Initialize the output.
    if (posix_spawnattr_init(attr) != 0) {
        return false;
    }

    if (posix_spawn_file_actions_init(actions) != 0) {
        posix_spawnattr_destroy(attr);
        return false;
    }

    bool should_set_parent_group_id = false;
    int desired_parent_group_id = 0;
    if (job_get_flag(j, JOB_CONTROL)) {
        should_set_parent_group_id = true;

        // PCA: I'm quite fuzzy on process groups, but I believe that the default value of 0 means
        // that the process becomes its own group leader, which is what set_child_group did in this
        // case. So we want this to be 0 if j->pgid is 0.
        desired_parent_group_id = j->pgid;
    }

    // Set the handling for job control signals back to the default.
    bool reset_signal_handlers = true;

    // Remove all signal blocks.
    bool reset_sigmask = true;

    // Set our flags.
    short flags = 0;
    if (reset_signal_handlers) flags |= POSIX_SPAWN_SETSIGDEF;
    if (reset_sigmask) flags |= POSIX_SPAWN_SETSIGMASK;
    if (should_set_parent_group_id) flags |= POSIX_SPAWN_SETPGROUP;

    int err = 0;
    if (!err) err = posix_spawnattr_setflags(attr, flags);

    if (!err && should_set_parent_group_id)
        err = posix_spawnattr_setpgroup(attr, desired_parent_group_id);

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

    for (size_t idx = 0; idx < io_chain.size(); idx++) {
        const shared_ptr<const io_data_t> io = io_chain.at(idx);

        if (io->io_mode == IO_FD) {
            CAST_INIT(const io_fd_t *, io_fd, io.get());
            if (io->fd == io_fd->old_fd) continue;
        }

        switch (io->io_mode) {
            case IO_CLOSE: {
                if (!err) err = posix_spawn_file_actions_addclose(actions, io->fd);
                break;
            }

            case IO_FILE: {
                CAST_INIT(const io_file_t *, io_file, io.get());
                if (!err)
                    err = posix_spawn_file_actions_addopen(actions, io->fd, io_file->filename_cstr,
                                                           io_file->flags /* mode */, OPEN_MASK);
                break;
            }

            case IO_FD: {
                CAST_INIT(const io_fd_t *, io_fd, io.get());
                if (!err)
                    err = posix_spawn_file_actions_adddup2(actions, io_fd->old_fd /* from */,
                                                           io->fd /* to */);
                break;
            }

            case IO_BUFFER:
            case IO_PIPE: {
                CAST_INIT(const io_pipe_t *, io_pipe, io.get());
                unsigned int write_pipe_idx = (io_pipe->is_input ? 0 : 1);
                int from_fd = io_pipe->pipe_fd[write_pipe_idx];
                int to_fd = io->fd;
                if (!err) err = posix_spawn_file_actions_adddup2(actions, from_fd, to_fd);

                if (write_pipe_idx > 0) {
                    if (!err) err = posix_spawn_file_actions_addclose(actions, io_pipe->pipe_fd[0]);
                    if (!err) err = posix_spawn_file_actions_addclose(actions, io_pipe->pipe_fd[1]);
                } else {
                    if (!err) err = posix_spawn_file_actions_addclose(actions, io_pipe->pipe_fd[0]);
                }
                break;
            }
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
                sz += strlen(*p) + 1;
            }

            for (p = envv; *p; p++) {
                sz += strlen(*p) + 1;
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

            // debug(0, L"The file '%ls' is marked as an executable but could not be run by the
            // operating system.", p->actual_cmd);
            break;
        }
    }
}

/// Perform output from builtins. May be called from a forked child, so don't do anything that may
/// allocate memory, etc.
bool do_builtin_io(const char *out, size_t outlen, const char *err, size_t errlen) {
    bool success = true;
    if (out && outlen) {
        if (write_loop(STDOUT_FILENO, out, outlen) < 0) {
            int e = errno;
            debug_safe(0, "Error while writing to stdout");
            safe_perror("write_loop");
            success = false;
            errno = e;
        }
    }

    if (err && errlen) {
        if (write_loop(STDERR_FILENO, err, errlen) < 0) {
            success = false;
        }
    }
    return success;
}
