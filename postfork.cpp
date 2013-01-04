/** \file postfork.cpp

  Functions that we may safely call after fork().
*/

#include <fcntl.h>
#include "signal.h"
#include "postfork.h"
#include "iothread.h"
#include "exec.h"

/** The number of times to try to call fork() before giving up */
#define FORK_LAPS 5

/** The number of nanoseconds to sleep between attempts to call fork() */
#define FORK_SLEEP_TIME 1000000

/** Base open mode to pass to calls to open */
#define OPEN_MASK 0666

/** fork error message */
#define FORK_ERROR "Could not create child process - exiting"

/** file redirection clobbering error message */
#define NOCLOB_ERROR "The file '%s' already exists"

/** file redirection error message */
#define FILE_ERROR "An error occurred while redirecting file '%s'"

/** file descriptor redirection error message */
#define FD_ERROR   "An error occurred while redirecting file descriptor %s"

/** pipe error */
#define LOCAL_PIPE_ERROR "An error occurred while setting up pipe"

/* Cover for debug_safe that can take an int. The format string should expect a %s */
static void debug_safe_int(int level, const char *format, int val)
{
    char buff[128];
    format_long_safe(buff, val);
    debug_safe(level, format, buff);
}

// PCA These calls to debug are rather sketchy because they may allocate memory. Fortunately they only occur if an error occurs.
int set_child_group(job_t *j, process_t *p, int print_errors)
{
    int res = 0;

    if (job_get_flag(j, JOB_CONTROL))
    {
        if (!j->pgid)
        {
            j->pgid = p->pid;
        }

        if (setpgid(p->pid, j->pgid))
        {
            if (getpgid(p->pid) != j->pgid && print_errors)
            {
                char pid_buff[128];
                char job_id_buff[128];
                char getpgid_buff[128];
                char job_pgid_buff[128];

                format_long_safe(pid_buff, p->pid);
                format_long_safe(job_id_buff, j->job_id);
                format_long_safe(getpgid_buff, getpgid(p->pid));
                format_long_safe(job_pgid_buff, j->pgid);

                debug_safe(1,
                           "Could not send process %s, '%s' in job %s, '%s' from group %s to group %s",
                           pid_buff,
                           p->argv0_cstr(),
                           job_id_buff,
                           j->command_cstr(),
                           getpgid_buff,
                           job_pgid_buff);

                wperror(L"setpgid");
                res = -1;
            }
        }
    }
    else
    {
        j->pgid = getpid();
    }

    if (job_get_flag(j, JOB_TERMINAL) && job_get_flag(j, JOB_FOREGROUND))
    {
        if (tcsetpgrp(0, j->pgid) && print_errors)
        {
            char job_id_buff[128];
            format_long_safe(job_id_buff, j->job_id);
            debug_safe(1, "Could not send job %s ('%s') to foreground", job_id_buff, j->command_cstr());
            wperror(L"tcsetpgrp");
            res = -1;
        }
    }

    return res;
}

/** Make sure the fd used by each redirection is not used by a pipe.  */
static void free_redirected_fds_from_pipes(io_chain_t &io_chain)
{
    size_t max = io_chain.size();
    for (size_t i = 0; i < max; i++)
    {
        int fd_to_free = io_chain.at(i)->fd;

        /* We only have to worry about fds beyond the three standard ones */
        if (fd_to_free <= 2)
            continue;

        /* Make sure the fd is not used by a pipe */
        for (size_t j = 0; j < max; j++)
        {
            /* We're only interested in pipes */
            shared_ptr<io_data_t> possible_conflict = io_chain.at(j);
            if (possible_conflict->io_mode != IO_PIPE && possible_conflict->io_mode != IO_BUFFER)
                continue;

            /* If the pipe is a conflict, dup it to some other value */
            for (int k=0; k<2; k++)
            {
                /* If it's not a conflict, we don't care */
                if (possible_conflict->param1.pipe_fd[k] != fd_to_free)
                    continue;

                /* Repeat until we have a replacement fd */
                int replacement_fd = -1;
                while (replacement_fd < 0)
                {
                    replacement_fd = dup(fd_to_free);
                    if (replacement_fd == -1 && errno != EINTR)
                    {
                        debug_safe_int(1, FD_ERROR, fd_to_free);
                        wperror(L"dup");
                        FATAL_EXIT();
                    }
                }
                possible_conflict->param1.pipe_fd[k] = replacement_fd;
            }
        }
    }
}


/**
   Set up a childs io redirections. Should only be called by
   setup_child_process(). Does the following: First it closes any open
   file descriptors not related to the child by calling
   close_unused_internal_pipes() and closing the universal variable
   server file descriptor. It then goes on to perform all the
   redirections described by \c io.

   \param io the list of IO redirections for the child

   \return 0 on sucess, -1 on failiure
*/
static int handle_child_io(io_chain_t &io_chain)
{

    close_unused_internal_pipes(io_chain);
    free_redirected_fds_from_pipes(io_chain);
    for (size_t idx = 0; idx < io_chain.size(); idx++)
    {
        shared_ptr<io_data_t> io = io_chain.at(idx);
        int tmp;

        if (io->io_mode == IO_FD && io->fd == io->param1.old_fd)
        {
            continue;
        }

        switch (io->io_mode)
        {
            case IO_CLOSE:
            {
                if (close(io->fd))
                {
                    debug_safe_int(0, "Failed to close file descriptor %s", io->fd);
                    wperror(L"close");
                }
                break;
            }

            case IO_FILE:
            {
                // Here we definitely do not want to set CLO_EXEC because our child needs access
                if ((tmp=open(io->filename_cstr,
                              io->param2.flags, OPEN_MASK))==-1)
                {
                    if ((io->param2.flags & O_EXCL) &&
                            (errno ==EEXIST))
                    {
                        debug_safe(1, NOCLOB_ERROR, io->filename_cstr);
                    }
                    else
                    {
                        debug_safe(1, FILE_ERROR, io->filename_cstr);
                        perror("open");
                    }

                    return -1;
                }
                else if (tmp != io->fd)
                {
                    /*
                      This call will sometimes fail, but that is ok,
                      this is just a precausion.
                    */
                    close(io->fd);

                    if (dup2(tmp, io->fd) == -1)
                    {
                        debug_safe_int(1,  FD_ERROR, io->fd);
                        perror("dup2");
                        return -1;
                    }
                    exec_close(tmp);
                }
                break;
            }

            case IO_FD:
            {
                /*
                  This call will sometimes fail, but that is ok,
                  this is just a precausion.
                */
                close(io->fd);

                if (dup2(io->param1.old_fd, io->fd) == -1)
                {
                    debug_safe_int(1, FD_ERROR, io->fd);
                    wperror(L"dup2");
                    return -1;
                }
                break;
            }

            case IO_BUFFER:
            case IO_PIPE:
            {
                /* If write_pipe_idx is 0, it means we're connecting to the read end (first pipe fd). If it's 1, we're connecting to the write end (second pipe fd). */
                unsigned int write_pipe_idx = (io->is_input ? 0 : 1);
                /*
                        debug( 0,
                             L"%ls %ls on fd %d (%d %d)",
                             write_pipe?L"write":L"read",
                             (io->io_mode == IO_BUFFER)?L"buffer":L"pipe",
                             io->fd,
                             io->param1.pipe_fd[0],
                             io->param1.pipe_fd[1]);
                */
                if (dup2(io->param1.pipe_fd[write_pipe_idx], io->fd) != io->fd)
                {
                    debug_safe(1, LOCAL_PIPE_ERROR);
                    perror("dup2");
                    return -1;
                }

                if (io->param1.pipe_fd[0] >= 0)
                    exec_close(io->param1.pipe_fd[0]);
                if (io->param1.pipe_fd[1] >= 0)
                    exec_close(io->param1.pipe_fd[1]);
                break;
            }

        }
    }

    return 0;

}


int setup_child_process(job_t *j, process_t *p)
{
    bool ok=true;

    if (p)
    {
        ok = (0 == set_child_group(j, p, 1));
    }

    if (ok)
    {
        ok = (0 == handle_child_io(j->io));
        if (p != 0 && ! ok)
        {
            exit_without_destructors(1);
        }
    }

    /* Set the handling for job control signals back to the default.  */
    if (ok)
    {
        signal_reset_handlers();
    }

    /* Remove all signal blocks */
    signal_unblock();

    return ok ? 0 : -1;
}

int g_fork_count = 0;

/**
   This function is a wrapper around fork. If the fork calls fails
   with EAGAIN, it is retried FORK_LAPS times, with a very slight
   delay between each lap. If fork fails even then, the process will
   exit with an error message.
*/
pid_t execute_fork(bool wait_for_threads_to_die)
{
    ASSERT_IS_MAIN_THREAD();

    if (wait_for_threads_to_die)
    {
        /* Make sure we have no outstanding threads before we fork. This is a pretty sketchy thing to do here, both because exec.cpp shouldn't have to know about iothreads, and because the completion handlers may do unexpected things. */
        iothread_drain_all();
    }

    pid_t pid;
    struct timespec pollint;
    int i;

    g_fork_count++;

    for (i=0; i<FORK_LAPS; i++)
    {
        pid = fork();
        if (pid >= 0)
        {
            return pid;
        }

        if (errno != EAGAIN)
        {
            break;
        }

        pollint.tv_sec = 0;
        pollint.tv_nsec = FORK_SLEEP_TIME;

        /*
          Don't sleep on the final lap - sleeping might change the
          value of errno, which will break the error reporting below.
        */
        if (i != FORK_LAPS-1)
        {
            nanosleep(&pollint, NULL);
        }
    }

    debug_safe(0, FORK_ERROR);
    wperror(L"fork");
    FATAL_EXIT();
    return 0;
}

#if FISH_USE_POSIX_SPAWN
bool fork_actions_make_spawn_properties(posix_spawnattr_t *attr, posix_spawn_file_actions_t *actions, job_t *j, process_t *p)
{
    /* Initialize the output */
    if (posix_spawnattr_init(attr) != 0)
    {
        return false;
    }

    if (posix_spawn_file_actions_init(actions) != 0)
    {
        posix_spawnattr_destroy(attr);
        return false;
    }

    bool should_set_parent_group_id = false;
    int desired_parent_group_id = 0;
    if (job_get_flag(j, JOB_CONTROL))
    {
        should_set_parent_group_id = true;

        // PCA: I'm quite fuzzy on process groups,
        // but I believe that the default value of 0
        // means that the process becomes its own
        // group leader, which is what set_child_group did
        // in this case. So we want this to be 0 if j->pgid is 0.
        desired_parent_group_id = j->pgid;
    }

    /* Set the handling for job control signals back to the default.  */
    bool reset_signal_handlers = true;

    /* Remove all signal blocks */
    bool reset_sigmask = true;

    /* Set our flags */
    short flags = 0;
    if (reset_signal_handlers)
        flags |= POSIX_SPAWN_SETSIGDEF;
    if (reset_sigmask)
        flags |= POSIX_SPAWN_SETSIGMASK;
    if (should_set_parent_group_id)
        flags |= POSIX_SPAWN_SETPGROUP;

    int err = 0;
    if (! err)
        err = posix_spawnattr_setflags(attr, flags);

    if (! err && should_set_parent_group_id)
        err = posix_spawnattr_setpgroup(attr, desired_parent_group_id);

    /* Everybody gets default handlers */
    if (! err && reset_signal_handlers)
    {
        sigset_t sigdefault;
        get_signals_with_handlers(&sigdefault);
        err = posix_spawnattr_setsigdefault(attr, &sigdefault);
    }

    /* No signals blocked */
    sigset_t sigmask;
    sigemptyset(&sigmask);
    if (! err && reset_sigmask)
        err = posix_spawnattr_setsigmask(attr, &sigmask);

    /* Make sure that our pipes don't use an fd that the redirection itself wants to use */
    free_redirected_fds_from_pipes(j->io);

    /* Close unused internal pipes */
    std::vector<int> files_to_close;
    get_unused_internal_pipes(files_to_close, j->io);
    for (size_t i = 0; ! err && i < files_to_close.size(); i++)
    {
        err = posix_spawn_file_actions_addclose(actions, files_to_close.at(i));
    }

    for (size_t idx = 0; idx < j->io.size(); idx++)
    {
        shared_ptr<const io_data_t> io = j->io.at(idx);

        if (io->io_mode == IO_FD && io->fd == io->param1.old_fd)
        {
            continue;
        }

        if (io->fd > 2)
        {
            /* Make sure the fd used by this redirection is not used by e.g. a pipe. */
            // free_fd(io_chain, io->fd );
            // PCA I don't think we need to worry about this. fd redirection is pretty uncommon anyways.
        }
        switch (io->io_mode)
        {
            case IO_CLOSE:
            {
                if (! err)
                    err = posix_spawn_file_actions_addclose(actions, io->fd);
                break;
            }

            case IO_FILE:
            {
                if (! err)
                    err = posix_spawn_file_actions_addopen(actions, io->fd, io->filename_cstr, io->param2.flags /* mode */, OPEN_MASK);
                break;
            }

            case IO_FD:
            {
                if (! err)
                    err = posix_spawn_file_actions_adddup2(actions, io->param1.old_fd /* from */, io->fd /* to */);
                break;
            }

            case IO_BUFFER:
            case IO_PIPE:
            {
                unsigned int write_pipe_idx = (io->is_input ? 0 : 1);
                int from_fd = io->param1.pipe_fd[write_pipe_idx];
                int to_fd = io->fd;
                if (! err)
                    err = posix_spawn_file_actions_adddup2(actions, from_fd, to_fd);


                if (write_pipe_idx > 0)
                {
                    if (! err)
                        err = posix_spawn_file_actions_addclose(actions, io->param1.pipe_fd[0]);
                    if (! err)
                        err = posix_spawn_file_actions_addclose(actions, io->param1.pipe_fd[1]);
                }
                else
                {
                    if (! err)
                        err = posix_spawn_file_actions_addclose(actions, io->param1.pipe_fd[0]);

                }
                break;
            }
        }
    }

    /* Clean up on error */
    if (err)
    {
        posix_spawnattr_destroy(attr);
        posix_spawn_file_actions_destroy(actions);
    }

    return ! err;
}
#endif //FISH_USE_POSIX_SPAWN

void safe_report_exec_error(int err, const char *actual_cmd, char **argv, char **envv)
{
    debug_safe(0, "Failed to execute process '%s'. Reason:", actual_cmd);

    switch (err)
    {

        case E2BIG:
        {
            char sz1[128], sz2[128];

            long arg_max = -1;

            size_t sz = 0;
            char **p;
            for (p=argv; *p; p++)
            {
                sz += strlen(*p)+1;
            }

            for (p=envv; *p; p++)
            {
                sz += strlen(*p)+1;
            }

            format_size_safe(sz1, sz);
            arg_max = sysconf(_SC_ARG_MAX);

            if (arg_max > 0)
            {
                format_size_safe(sz2, sz);
                debug_safe(0, "The total size of the argument and environment lists %s exceeds the operating system limit of %s.", sz1, sz2);
            }
            else
            {
                debug_safe(0, "The total size of the argument and environment lists (%s) exceeds the operating system limit.", sz1);
            }

            debug_safe(0, "Try running the command again with fewer arguments.");
            break;
        }

        case ENOEXEC:
        {
            /* Hope strerror doesn't allocate... */
            const char *err = strerror(errno);
            debug_safe(0, "exec: %s", err);

            debug_safe(0, "The file '%s' is marked as an executable but could not be run by the operating system.", actual_cmd);
            break;
        }

        case ENOENT:
        {
            /* ENOENT is returned by exec() when the path fails, but also returned by posix_spawn if an open file action fails. These cases appear to be impossible to distinguish. We address this by not using posix_spawn for file redirections, so all the ENOENTs we find must be errors from exec(). */
            char interpreter_buff[128] = {}, *interpreter;
            interpreter = get_interpreter(actual_cmd, interpreter_buff, sizeof interpreter_buff);
            if (interpreter && 0 != access(interpreter, X_OK))
            {
                debug_safe(0, "The file '%s' specified the interpreter '%s', which is not an executable command.", actual_cmd, interpreter);
            }
            else
            {
                debug_safe(0, "The file '%s' does not exist or could not be executed.", actual_cmd);
            }
            break;
        }

        case ENOMEM:
        {
            debug_safe(0, "Out of memory");
            break;
        }

        default:
        {
            /* Hope strerror doesn't allocate... */
            const char *err = strerror(errno);
            debug_safe(0, "exec: %s", err);

            //    debug(0, L"The file '%ls' is marked as an executable but could not be run by the operating system.", p->actual_cmd);
            break;
        }
    }
}
