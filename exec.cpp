/** \file exec.c
  Functions for executing a program.

  Some of the code in this file is based on code from the Glibc
  manual, though the changes performed have been massive.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <assert.h>
#include <dirent.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <memory>

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif

#include "fallback.h"
#include "util.h"
#include "iothread.h"
#include "postfork.h"

#include "common.h"
#include "wutil.h"
#include "proc.h"
#include "exec.h"
#include "parser.h"
#include "builtin.h"
#include "function.h"
#include "env.h"
#include "wildcard.h"
#include "sanity.h"
#include "expand.h"
#include "signal.h"

#include "parse_util.h"

/**
   file descriptor redirection error message
*/
#define FD_ERROR   _( L"An error occurred while redirecting file descriptor %d" )

/**
   file descriptor redirection error message
*/
#define WRITE_ERROR   _( L"An error occurred while writing output" )

/**
   file redirection error message
*/
#define FILE_ERROR _( L"An error occurred while redirecting file '%s'" )

/**
   Base open mode to pass to calls to open
*/
#define OPEN_MASK 0666

/**
   List of all pipes used by internal pipes. These must be closed in
   many situations in order to make sure that stray fds aren't lying
   around.

   Note this is used after fork, so we must not do anything that may allocate memory. Hopefully methods like open_fds.at() don't.
*/
static std::vector<bool> open_fds;

// Called in a forked child
static void exec_write_and_exit(int fd, const char *buff, size_t count, int status)
{
    if (write_loop(fd, buff, count) == -1)
    {
        debug(0, WRITE_ERROR);
        wperror(L"write");
        exit_without_destructors(status);
    }
    exit_without_destructors(status);
}


void exec_close(int fd)
{
    ASSERT_IS_MAIN_THREAD();

    /* This may be called in a child of fork(), so don't allocate memory */
    if (fd < 0)
    {
        debug(0, L"Called close on invalid file descriptor ");
        return;
    }

    while (close(fd) == -1)
    {
        if (errno != EINTR)
        {
            debug(1, FD_ERROR, fd);
            wperror(L"close");
            break;
        }
    }

    /* Maybe remove this from our set of open fds */
    if ((size_t)fd < open_fds.size())
    {
        open_fds[fd] = false;
    }
}

int exec_pipe(int fd[2])
{
    ASSERT_IS_MAIN_THREAD();

    int res;
    while ((res=pipe(fd)))
    {
        if (errno != EINTR)
        {
            // caller will call wperror
            return res;
        }
    }

    debug(4, L"Created pipe using fds %d and %d", fd[0], fd[1]);

    int max_fd = std::max(fd[0], fd[1]);
    if (max_fd >= 0 && open_fds.size() <= (size_t)max_fd)
    {
        open_fds.resize(max_fd + 1, false);
    }
    open_fds.at(fd[0]) = true;
    open_fds.at(fd[1]) = true;

    return res;
}

/* Returns true if the redirection is a file redirection to a file other than /dev/null */
static bool redirection_is_to_real_file(const io_data_t *io)
{
    bool result = false;
    if (io != NULL && io->io_mode == IO_FILE)
    {
        /* It's a file redirection. Compare the path to /dev/null */
        CAST_INIT(const io_file_t *, io_file, io);
        const char *path = io_file->filename_cstr;
        if (strcmp(path, "/dev/null") != 0)
        {
            /* It's not /dev/null */
            result = true;
        }

    }
    return result;
}

static bool chain_contains_redirection_to_real_file(const io_chain_t &io_chain)
{
    bool result = false;
    for (size_t idx=0; idx < io_chain.size(); idx++)
    {
        const shared_ptr<const io_data_t> &io = io_chain.at(idx);
        if (redirection_is_to_real_file(io.get()))
        {
            result = true;
            break;
        }
    }
    return result;
}

void print_open_fds(void)
{
    for (size_t i=0; i < open_fds.size(); ++i)
    {
        if (open_fds.at(i))
        {
            fprintf(stderr, "fd %lu\n", i);
        }
    }
}

/**
   Check if the specified fd is used as a part of a pipeline in the
   specidied set of IO redirections.
   This is called after fork().

   \param fd the fd to search for
   \param io_chain the set of io redirections to search in
*/
static bool use_fd_in_pipe(int fd, const io_chain_t &io_chain)
{
    for (size_t idx = 0; idx < io_chain.size(); idx++)
    {
        const shared_ptr<const io_data_t> &io = io_chain.at(idx);
        if ((io->io_mode == IO_BUFFER) ||
                (io->io_mode == IO_PIPE))
        {
            CAST_INIT(const io_pipe_t *, io_pipe, io.get());
            if (io_pipe->pipe_fd[0] == fd || io_pipe->pipe_fd[1] == fd)
                return true;
        }
    }
    return false;
}


/**
   Close all fds in open_fds, except for those that are mentioned in
   the redirection list io. This should make sure that there are no
   stray opened file descriptors in the child.

   \param io the list of io redirections for this job. Pipes mentioned
   here should not be closed.
*/
void close_unused_internal_pipes(const io_chain_t &io)
{
    /* A call to exec_close will modify open_fds, so be careful how we walk */
    for (size_t i=0; i < open_fds.size(); i++)
    {
        if (open_fds[i])
        {
            int fd = (int)i;
            if (!use_fd_in_pipe(fd, io))
            {
                debug(4, L"Close fd %d, used in other context", fd);
                exec_close(fd);
                i--;
            }
        }
    }
}

void get_unused_internal_pipes(std::vector<int> &fds, const io_chain_t &io)
{
    for (size_t i=0; i < open_fds.size(); i++)
    {
        if (open_fds[i])
        {
            int fd = (int)i;
            if (!use_fd_in_pipe(fd, io))
            {
                fds.push_back(fd);
            }
        }
    }
}

/**
   Returns the interpreter for the specified script. Returns NULL if file
   is not a script with a shebang.
 */
char *get_interpreter(const char *command, char *interpreter, size_t buff_size)
{
    // OK to not use CLO_EXEC here because this is only called after fork
    int fd = open(command, O_RDONLY);
    if (fd >= 0)
    {
        size_t idx = 0;
        while (idx + 1 < buff_size)
        {
            char ch;
            ssize_t amt = read(fd, &ch, sizeof ch);
            if (amt <= 0)
                break;
            if (ch == '\n')
                break;
            interpreter[idx++] = ch;
        }
        interpreter[idx++] = '\0';
        close(fd);
    }
    if (strncmp(interpreter, "#! /", 4) == 0)
    {
        return interpreter + 3;
    }
    else if (strncmp(interpreter, "#!/", 3) == 0)
    {
        return interpreter + 2;
    }
    else
    {
        return NULL;
    }
}

/**
   This function is executed by the child process created by a call to
   fork(). It should be called after \c setup_child_process. It calls
   execve to replace the fish process image with the command specified
   in \c p. It never returns.
*/
/* Called in a forked child! Do not allocate memory, etc. */
static void safe_launch_process(process_t *p, const char *actual_cmd, const char *const* cargv, const char *const *cenvv)
{
    int err;

//  debug( 1, L"exec '%ls'", p->argv[0] );

    // This function never returns, so we take certain liberties with constness
    char * const * envv = const_cast<char* const *>(cenvv);
    char * const * argv = const_cast<char* const *>(cargv);

    execve(actual_cmd, argv, envv);

    err = errno;

    /*
       Something went wrong with execve, check for a ":", and run
       /bin/sh if encountered. This is a weird predecessor to the shebang
       that is still sometimes used since it is supported on Windows.
    */
    /* OK to not use CLO_EXEC here because this is called after fork and the file is immediately closed */
    int fd = open(actual_cmd, O_RDONLY);
    if (fd >= 0)
    {
        char begin[1] = {0};
        ssize_t amt_read = read(fd, begin, 1);
        close(fd);

        if ((amt_read==1) && (begin[0] == ':'))
        {
            // Relaunch it with /bin/sh. Don't allocate memory, so if you have more args than this, update your silly script! Maybe this should be changed to be based on ARG_MAX somehow.
            char sh_command[] = "/bin/sh";
            char *argv2[128];
            argv2[0] = sh_command;
            for (size_t i=1; i < sizeof argv2 / sizeof *argv2; i++)
            {
                argv2[i] = argv[i-1];
                if (argv2[i] == NULL)
                    break;
            }

            execve(sh_command, argv2, envv);
        }
    }

    errno = err;
    safe_report_exec_error(errno, actual_cmd, argv, envv);
    exit_without_destructors(STATUS_EXEC_FAIL);
}

/**
   This function is similar to launch_process, except it is not called after a fork (i.e. it only calls exec) and therefore it can allocate memory.
*/
static void launch_process_nofork(process_t *p)
{
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

    char **argv = wcsv2strv(p->get_argv());
    const char *const *envv = env_export_arr(false);
    char *actual_cmd = wcs2str(p->actual_cmd.c_str());

    /* Bounce to launch_process. This never returns. */
    safe_launch_process(p, actual_cmd, argv, envv);
}


/**
   Check if the IO redirection chains contains redirections for the
   specified file descriptor
*/
static int has_fd(const io_chain_t &d, int fd)
{
    return io_chain_get(d, fd).get() != NULL;
}

/**
   Close a list of fds.
*/
static void io_cleanup_fds(const std::vector<int> &opened_fds)
{
    std::for_each(opened_fds.begin(), opened_fds.end(), close);
}

/**
   Make a copy of the specified io redirection chain, but change file
   redirection into fd redirection. This makes the redirection chain
   suitable for use as block-level io, since the file won't be
   repeatedly reopened for every command in the block, which would
   reset the cursor position.

   \return the transmogrified chain on sucess, or 0 on failiure
*/
static bool io_transmogrify(const io_chain_t &in_chain, io_chain_t &out_chain, std::vector<int> &out_opened_fds)
{
    ASSERT_IS_MAIN_THREAD();
    assert(out_chain.empty());

    /* Just to be clear what we do for an empty chain */
    if (in_chain.empty())
    {
        return true;
    }

    bool success = true;

    /* Make our chain of redirections */
    io_chain_t result_chain;

    /* In the event we can't finish transmorgrifying, we'll have to close all the files we opened. */
    std::vector<int> opened_fds;

    for (size_t idx = 0; idx < in_chain.size(); idx++)
    {
        const shared_ptr<io_data_t> &in = in_chain.at(idx);
        shared_ptr<io_data_t> out; //gets allocated via new

        switch (in->io_mode)
        {
            default:
                /* Unknown type, should never happen */
                fprintf(stderr, "Unknown io_mode %ld\n", (long)in->io_mode);
                abort();
                break;

                /*
                  These redirections don't need transmogrification. They can be passed through.
                */
            case IO_PIPE:
            case IO_FD:
            case IO_BUFFER:
            case IO_CLOSE:
            {
                out = in;
                break;
            }

            /*
              Transmogrify file redirections
            */
            case IO_FILE:
            {
                int fd;
                CAST_INIT(io_file_t *, in_file, in.get());
                if ((fd=open(in_file->filename_cstr, in_file->flags, OPEN_MASK))==-1)
                {
                    debug(1,
                          FILE_ERROR,
                          in_file->filename_cstr);

                    wperror(L"open");
                    success = false;
                    break;
                }

                opened_fds.push_back(fd);
                out.reset(new io_fd_t(in->fd, fd, true));

                break;
            }
        }

        if (out.get() != NULL)
            result_chain.push_back(out);

        /* Don't go any further if we failed */
        if (! success)
        {
            break;
        }
    }

    /* Now either return success, or clean up */
    if (success)
    {
        /* Yay */
        out_chain.swap(result_chain);
        out_opened_fds.swap(opened_fds);
    }
    else
    {
        /* No dice - clean up */
        result_chain.clear();
        io_cleanup_fds(opened_fds);
    }
    return success;
}


/**
   Morph an io redirection chain into redirections suitable for
   passing to eval, call eval, and clean up morphed redirections.

   \param def the code to evaluate
   \param block_type the type of block to push on evaluation
   \param io the io redirections to be performed on this block
*/

static void internal_exec_helper(parser_t &parser,
                                 const wchar_t *def,
                                 enum block_type_t block_type,
                                 const io_chain_t &ios)
{
    io_chain_t morphed_chain;
    std::vector<int> opened_fds;
    bool transmorgrified = io_transmogrify(ios, morphed_chain, opened_fds);

    int is_block_old=is_block;
    is_block=1;

    /*
      Did the transmogrification fail - if so, set error status and return
    */
    if (! transmorgrified)
    {
        proc_set_last_status(STATUS_EXEC_FAIL);
        return;
    }

    signal_unblock();

    parser.eval(def, morphed_chain, block_type);

    signal_block();

    morphed_chain.clear();
    io_cleanup_fds(opened_fds);
    job_reap(0);
    is_block=is_block_old;
}

/* Returns whether we can use posix spawn for a given process in a given job.
 Per https://github.com/fish-shell/fish-shell/issues/364 , error handling for file redirections is too difficult with posix_spawn,
 so in that case we use fork/exec.

 Furthermore, to avoid the race between the caller calling tcsetpgrp() and the client checking the foreground process group, we don't use posix_spawn if we're going to foreground the process. (If we use fork(), we can call tcsetpgrp after the fork, before the exec, and avoid the race).
*/
static bool can_use_posix_spawn_for_job(const job_t *job, const process_t *process)
{
    if (job_get_flag(job, JOB_CONTROL))
    {
        /* We are going to use job control; therefore when we launch this job it will get its own process group ID. But will it be foregrounded? */
        if (job_get_flag(job, JOB_TERMINAL) && job_get_flag(job, JOB_FOREGROUND))
        {
            /* It will be foregrounded, so we will call tcsetpgrp(), therefore do not use posix_spawn */
            return false;
        }
    }
    
    /* Now see if we have a redirection involving a file. The only one we allow is /dev/null, which we assume will not fail. */
    bool result = true;
    if (chain_contains_redirection_to_real_file(job->block_io_chain()) || chain_contains_redirection_to_real_file(process->io_chain()))
    {
        result = false;
    }
    return result;
}

/* What exec does if no_exec is set. This only has to handle block pushing and popping. See #624. */
static void exec_no_exec(parser_t &parser, const job_t *job)
{
    /* Hack hack hack. If this is an 'end' job, then trigger a pop. If this is a job that would create a block, trigger a push. See #624 */
    const process_t *p = job->first_process;
    if (p && p->type == INTERNAL_BUILTIN)
    {
        const wchar_t *builtin_name_cstr = p->argv0();
        if (builtin_name_cstr != NULL)
        {
            const wcstring builtin_name = builtin_name_cstr;
            if (contains(builtin_name, L"for", L"function", L"begin", L"switch"))
            {
                // The above builtins are the ones that produce an unbalanced block from within their function implementation
                // This list should be maintained somewhere else
                parser.push_block(new fake_block_t());
            }
            else if (builtin_name == L"end")
            {
                if (parser.current_block == NULL || parser.current_block->type() == TOP)
                {
                    fprintf(stderr, "Warning: not popping the root block\n");
                }
                else
                {
                    parser.pop_block();
                }
            }
        }
    }
}

void exec_job(parser_t &parser, job_t *j)
{
    pid_t pid = 0;
    sigset_t chldset;

    /*
      Set to true if something goes wrong while exec:ing the job, in
      which case the cleanup code will kick in.
    */
    bool exec_error = false;

    bool needs_keepalive = false;
    process_t keepalive;


    CHECK(j,);
    CHECK_BLOCK();

    if (no_exec)
    {
        exec_no_exec(parser, j);
        return;
    }

    sigemptyset(&chldset);
    sigaddset(&chldset, SIGCHLD);

    debug(4, L"Exec job '%ls' with id %d", j->command_wcstr(), j->job_id);

    /* PCA Here we detect the special case of an input buffer redirection, i.e. we want a process to receive data that we hold in a buffer (it is an INPUT for the process, but an output for fish). This is extremely rare: I believe only run_pager creates these and it would be nice to dump it. So we can only have at most one.

        It would be great to wean fish_pager off of input redirections so that we can dump input redirections and the INTERNAL_BUFFER process type altogether.
      */
    const io_buffer_t *single_magic_input_redirect = NULL;
    const io_chain_t all_ios = j->all_io_redirections();
    for (size_t idx = 0; idx < all_ios.size(); idx++)
    {
        const shared_ptr<io_data_t> &io = all_ios.at(idx);

        if ((io->io_mode == IO_BUFFER))
        {
            CAST_INIT(io_buffer_t *, io_buffer, io.get());
            if (io_buffer->is_input)
            {
                /* We expect to have at most one of these, per the comment above. Note that this assertion is the only reason we don't break out of the loop below  */
                assert(single_magic_input_redirect == NULL && "Should have at most one input IO_BUFFER");

                /*
                  Input redirection - create a new gobetween process to take
                  care of buffering, save the redirection in input_redirect
                */
                process_t *fake = new process_t();
                fake->type  = INTERNAL_BUFFER;
                fake->pipe_write_fd = STDOUT_FILENO;
                j->first_process->pipe_read_fd = io->fd;
                fake->next = j->first_process;
                j->first_process = fake;
                single_magic_input_redirect = io_buffer;
            }
        }
    }

    if (j->first_process->type==INTERNAL_EXEC)
    {
        /*
          Do a regular launch -  but without forking first...
        */
        signal_block();

        /*
          setup_child_process makes sure signals are properly set
          up. It will also call signal_unblock
        */

        /* PCA This is for handling exec. Passing all_ios here matches what fish 2.0.0 and 1.x did. It's known to be wrong - for example, it means that redirections bound for subsequent commands in the pipeline will apply to exec. However, using exec in a pipeline doesn't really make sense, so I'm not trying to fix it here. */
        if (!setup_child_process(j, 0, all_ios))
        {
            /*
              launch_process _never_ returns
            */
            launch_process_nofork(j->first_process);
        }
        else
        {
            job_set_flag(j, JOB_CONSTRUCTED, 1);
            j->first_process->completed=1;
            return;
        }

    }

    signal_block();

    /*
      See if we need to create a group keepalive process. This is
      a process that we create to make sure that the process group
      doesn't die accidentally, and is often needed when a
      builtin/block/function is inside a pipeline, since that
      usually means we have to wait for one program to exit before
      continuing in the pipeline, causing the group leader to
      exit.
    */

    if (job_get_flag(j, JOB_CONTROL))
    {
        for (const process_t *p = j->first_process; p; p = p->next)
        {
            if (p->type != EXTERNAL)
            {
                if (p->next)
                {
                    needs_keepalive = true;
                    break;
                }
                if (p != j->first_process)
                {
                    needs_keepalive = true;
                    break;
                }

            }

        }
    }

    if (needs_keepalive)
    {
        /* Call fork. No need to wait for threads since our use is confined and simple. */
        if (g_log_forks)
        {
            printf("fork #%d: Executing keepalive fork for '%ls'\n", g_fork_count, j->command_wcstr());
        }
        keepalive.pid = execute_fork(false);
        if (keepalive.pid == 0)
        {
            /* Child */
            keepalive.pid = getpid();
            set_child_group(j, &keepalive, 1);
            pause();
            exit_without_destructors(0);
        }
        else
        {
            /* Parent */
            set_child_group(j, &keepalive, 0);
        }
    }

    /*
      This loop loops over every process_t in the job, starting it as
      appropriate. This turns out to be rather complex, since a
      process_t can be one of many rather different things.

      The loop also has to handle pipelining between the jobs.
    */

    /* We can have up to three pipes "in flight" at a time:

    1. The pipe the current process should read from (courtesy of the previous process)
    2. The pipe that the current process should write to
    3. The pipe that the next process should read from (courtesy of us)

    We are careful to set these to -1 when closed, so if we exit the loop abruptly, we can still close them.

    */

    int pipe_current_read = -1, pipe_current_write = -1, pipe_next_read = -1;
    for (process_t *p=j->first_process; p; p = p->next)
    {
        /* The IO chain for this process. It starts with the block IO, then pipes, and then gets any from the process */
        io_chain_t process_net_io_chain = j->block_io_chain();

        /* "Consume" any pipe_next_read by making it current */
        assert(pipe_current_read == -1);
        pipe_current_read = pipe_next_read;
        pipe_next_read = -1;

        /* See if we need a pipe */
        const bool pipes_to_next_command = (p->next != NULL);

        /* The pipes the current process write to and read from.
           Unfortunately these can't be just allocated on the stack, since
           j->io wants shared_ptr.

          The write pipe (destined for stdout) needs to occur before redirections. For example, with a redirection like this:
            `foo 2>&1 | bar`, what we want to happen is this:

            dup2(pipe, stdout)
            dup2(stdout, stderr)

            so that stdout and stderr both wind up referencing the pipe.

            The read pipe (destined for stdin) is more ambiguous. Imagine a pipeline like this:

               echo alpha | cat < beta.txt

            Should cat output alpha or beta? bash and ksh output 'beta', tcsh gets it right and complains about ambiguity, and zsh outputs both (!). No shells appear to output 'alpha', so we match bash here. That would mean putting the pipe first, so that it gets trumped by the file redirection.
            
            However, eval does this:
            
               echo "begin; $argv "\n" ;end eval2_inner <&3 3<&-" | source 3<&0
               
            which depends on the redirection being evaluated before the pipe. So the write end of the pipe comes first, the read pipe of the pipe comes last. See issue #966.
        */

        shared_ptr<io_pipe_t> pipe_write;
        shared_ptr<io_pipe_t> pipe_read;

        /* Write pipe goes first */
        if (p->next)
        {
            pipe_write.reset(new io_pipe_t(p->pipe_write_fd, false));
            process_net_io_chain.push_back(pipe_write);

        }

        /* The explicit IO redirections associated with the process */
        process_net_io_chain.append(p->io_chain());
        
        /* Read pipe goes last */
        if (p != j->first_process)
        {
            pipe_read.reset(new io_pipe_t(p->pipe_read_fd, true));
            /* Record the current read in pipe_read */
            pipe_read->pipe_fd[0] = pipe_current_read;
            process_net_io_chain.push_back(pipe_read);
        }
        

        /*
           This call is used so the global environment variable array
           is regenerated, if needed, before the fork. That way, we
           avoid a lot of duplicate work where EVERY child would need
           to generate it, since that result would not get written
           back to the parent. This call could be safely removed, but
           it would result in slightly lower performance - at least on
           uniprocessor systems.
        */
        if (p->type == EXTERNAL)
            env_export_arr(true);


        /*
          Set up fd:s that will be used in the pipe
        */

        if (pipes_to_next_command)
        {
//      debug( 1, L"%ls|%ls" , p->argv[0], p->next->argv[0]);
            int local_pipe[2] = {-1, -1};
            if (exec_pipe(local_pipe) == -1)
            {
                debug(1, PIPE_ERROR);
                wperror(L"pipe");
                exec_error = true;
                job_mark_process_as_failed(j, p);
                break;
            }

            // This tells the redirection about the fds, but the redirection does not close them
            memcpy(pipe_write->pipe_fd, local_pipe, sizeof(int)*2);

            // Record our pipes
            // The fds should be negative to indicate that we aren't overwriting an fd we failed to close
            assert(pipe_current_write == -1);
            pipe_current_write = local_pipe[1];

            assert(pipe_next_read == -1);
            pipe_next_read = local_pipe[0];
        }

        //fprintf(stderr, "before IO: ");
        //io_print(j->io);

        // This is the IO buffer we use for storing the output of a block or function when it is in a pipeline
        shared_ptr<io_buffer_t> block_output_io_buffer;
        switch (p->type)
        {
            case INTERNAL_FUNCTION:
            {
                /*
                  Calls to function_get_definition might need to
                  source a file as a part of autoloading, hence there
                  must be no blocks.
                */

                signal_unblock();
                wcstring def;
                bool function_exists = function_get_definition(p->argv0(), &def);

                wcstring_list_t named_arguments = function_get_named_arguments(p->argv0());
                bool shadows = function_get_shadows(p->argv0());

                signal_block();

                if (! function_exists)
                {
                    debug(0, _(L"Unknown function '%ls'"), p->argv0());
                    break;
                }
                function_block_t *newv = new function_block_t(p, p->argv0(), shadows);
                parser.push_block(newv);

                /*
                  set_argv might trigger an event
                  handler, hence we need to unblock
                  signals.
                */
                signal_unblock();
                parse_util_set_argv(p->get_argv()+1, named_arguments);
                signal_block();

                parser.forbid_function(p->argv0());

                if (p->next)
                {
                    // Be careful to handle failure, e.g. too many open fds
                    block_output_io_buffer.reset(io_buffer_t::create(false /* = not input */, STDOUT_FILENO));
                    if (block_output_io_buffer.get() == NULL)
                    {
                        exec_error = true;
                        job_mark_process_as_failed(j, p);
                    }
                    else
                    {
                        /* This looks sketchy, because we're adding this io buffer locally - they aren't in the process or job redirection list. Therefore select_try won't be able to read them. However we call block_output_io_buffer->read() below, which reads until EOF. So there's no need to select on this. */
                        process_net_io_chain.push_back(block_output_io_buffer);
                    }
                }

                if (! exec_error)
                {
                    internal_exec_helper(parser, def.c_str(), TOP, process_net_io_chain);
                }

                parser.allow_function();
                parser.pop_block();

                break;
            }

            case INTERNAL_BLOCK:
            {
                if (p->next)
                {
                    block_output_io_buffer.reset(io_buffer_t::create(0));
                    if (block_output_io_buffer.get() == NULL)
                    {
                        exec_error = true;
                        job_mark_process_as_failed(j, p);
                    }
                    else
                    {
                        /* See the comment above about it's OK to add an IO redirection to this local buffer, even though it won't be handled in select_try */
                        process_net_io_chain.push_back(block_output_io_buffer);
                    }
                }

                if (! exec_error)
                {
                    internal_exec_helper(parser, p->argv0(), TOP, process_net_io_chain);
                }
                break;

            }

            case INTERNAL_BUILTIN:
            {
                int builtin_stdin=0;
                bool close_stdin = false;

                /*
                  If this is the first process, check the io
                  redirections and see where we should be reading
                  from.
                */
                if (p == j->first_process)
                {
                    const shared_ptr<const io_data_t> in = process_net_io_chain.get_io_for_fd(STDIN_FILENO);

                    if (in)
                    {
                        switch (in->io_mode)
                        {

                            case IO_FD:
                            {
                                CAST_INIT(const io_fd_t *, in_fd, in.get());
                                builtin_stdin = in_fd->old_fd;
                                break;
                            }
                            case IO_PIPE:
                            {
                                CAST_INIT(const io_pipe_t *, in_pipe, in.get());
                                builtin_stdin = in_pipe->pipe_fd[0];
                                break;
                            }

                            case IO_FILE:
                            {
                                /* Do not set CLO_EXEC because child needs access */
                                CAST_INIT(const io_file_t *, in_file, in.get());
                                builtin_stdin=open(in_file->filename_cstr,
                                                   in_file->flags, OPEN_MASK);
                                if (builtin_stdin == -1)
                                {
                                    debug(1,
                                          FILE_ERROR,
                                          in_file->filename_cstr);
                                    wperror(L"open");
                                }
                                else
                                {
                                    close_stdin = true;
                                }

                                break;
                            }

                            case IO_CLOSE:
                            {
                                /*
                                  FIXME:

                                  When requesting that stdin be closed, we
                                  really don't do anything. How should this be
                                  handled?
                                 */
                                builtin_stdin = -1;

                                break;
                            }

                            default:
                            {
                                builtin_stdin=-1;
                                debug(1,
                                      _(L"Unknown input redirection type %d"),
                                      in->io_mode);
                                break;
                            }

                        }
                    }
                }
                else
                {
                    builtin_stdin = pipe_read->pipe_fd[0];
                }

                if (builtin_stdin == -1)
                {
                    exec_error = true;
                    break;
                }
                else
                {
                    int old_out = builtin_out_redirect;
                    int old_err = builtin_err_redirect;

                    /*
                       Since this may be the foreground job, and since
                       a builtin may execute another foreground job,
                       we need to pretend to suspend this job while
                       running the builtin, in order to avoid a
                       situation where two jobs are running at once.

                       The reason this is done here, and not by the
                       relevant builtins, is that this way, the
                       builtin does not need to know what job it is
                       part of. It could probably figure that out by
                       walking the job list, but it seems more robust
                       to make exec handle things.
                    */

                    builtin_push_io(parser, builtin_stdin);

                    builtin_out_redirect = has_fd(process_net_io_chain, STDOUT_FILENO);
                    builtin_err_redirect = has_fd(process_net_io_chain, STDERR_FILENO);

                    const int fg = job_get_flag(j, JOB_FOREGROUND);
                    job_set_flag(j, JOB_FOREGROUND, 0);

                    signal_unblock();

                    p->status = builtin_run(parser, p->get_argv(), process_net_io_chain);

                    builtin_out_redirect=old_out;
                    builtin_err_redirect=old_err;

                    signal_block();

                    /*
                      Restore the fg flag, which is temporarily set to
                      false during builtin execution so as not to confuse
                      some job-handling builtins.
                    */
                    job_set_flag(j, JOB_FOREGROUND, fg);
                }

                /*
                  If stdin has been redirected, close the redirection
                  stream.
                */
                if (close_stdin)
                {
                    exec_close(builtin_stdin);
                }
                break;
            }
        }

        if (exec_error)
        {
            break;
        }

        switch (p->type)
        {

            case INTERNAL_BLOCK:
            case INTERNAL_FUNCTION:
            {
                int status = proc_get_last_status();

                /*
                  Handle output from a block or function. This usually
                  means do nothing, but in the case of pipes, we have
                  to buffer such io, since otherwise the internal pipe
                  buffer might overflow.
                */
                if (! block_output_io_buffer.get())
                {
                    /*
                      No buffer, so we exit directly. This means we
                      have to manually set the exit status.
                    */
                    if (p->next == 0)
                    {
                        proc_set_last_status(job_get_flag(j, JOB_NEGATE)?(!status):status);
                    }
                    p->completed = 1;
                    break;
                }

                // Here we must have a non-NULL block_output_io_buffer
                assert(block_output_io_buffer.get() != NULL);
                io_remove(process_net_io_chain, block_output_io_buffer);

                block_output_io_buffer->read();

                const char *buffer = block_output_io_buffer->out_buffer_ptr();
                size_t count = block_output_io_buffer->out_buffer_size();

                if (block_output_io_buffer->out_buffer_size() > 0)
                {
                    /* We don't have to drain threads here because our child process is simple */
                    if (g_log_forks)
                    {
                        printf("Executing fork for internal block or function for '%ls'\n", p->argv0());
                    }
                    pid = execute_fork(false);
                    if (pid == 0)
                    {

                        /*
                          This is the child process. Write out the contents of the pipeline.
                        */
                        p->pid = getpid();
                        setup_child_process(j, p, process_net_io_chain);

                        exec_write_and_exit(block_output_io_buffer->fd, buffer, count, status);
                    }
                    else
                    {
                        /*
                           This is the parent process. Store away
                           information on the child, and possibly give
                           it control over the terminal.
                        */
                        p->pid = pid;
                        set_child_group(j, p, 0);

                    }

                }
                else
                {
                    if (p->next == 0)
                    {
                        proc_set_last_status(job_get_flag(j, JOB_NEGATE)?(!status):status);
                    }
                    p->completed = 1;
                }

                block_output_io_buffer.reset();
                break;

            }


            case INTERNAL_BUFFER:
            {
                assert(single_magic_input_redirect != NULL);
                const char *buffer = single_magic_input_redirect->out_buffer_ptr();
                size_t count = single_magic_input_redirect->out_buffer_size();

                /* We don't have to drain threads here because our child process is simple */
                if (g_log_forks)
                {
                    printf("fork #%d: Executing fork for internal buffer for '%ls'\n", g_fork_count, p->argv0() ? p->argv0() : L"(null)");
                }
                pid = execute_fork(false);
                if (pid == 0)
                {
                    /*
                      This is the child process. Write out the
                      contents of the pipeline.
                    */
                    p->pid = getpid();
                    setup_child_process(j, p, process_net_io_chain);

                    exec_write_and_exit(1, buffer, count, 0);
                }
                else
                {
                    /*
                       This is the parent process. Store away
                       information on the child, and possibly give
                       it control over the terminal.
                    */
                    p->pid = pid;
                    set_child_group(j, p, 0);
                }

                break;
            }

            case INTERNAL_BUILTIN:
            {
                /*
                  Handle output from builtin commands. In the general
                  case, this means forking of a worker process, that
                  will write out the contents of the stdout and stderr
                  buffers to the correct file descriptor. Since
                  forking is expensive, fish tries to avoid it when
                  possible.
                */
                
                bool fork_was_skipped = false;
                
                const shared_ptr<io_data_t> stdout_io = process_net_io_chain.get_io_for_fd(STDOUT_FILENO);
                const shared_ptr<io_data_t> stderr_io = process_net_io_chain.get_io_for_fd(STDERR_FILENO);
                
                /* If we are outputting to a file, we have to actually do it, even if we have no output, so that we can truncate the file. Does not apply to /dev/null. */
                bool must_fork = redirection_is_to_real_file(stdout_io.get()) || redirection_is_to_real_file(stderr_io.get());
                if (! must_fork)
                {   
                    if (p->next == NULL)
                    {
                        const bool stdout_is_to_buffer = stdout_io && stdout_io->io_mode == IO_BUFFER;
                        const bool no_stdout_output =  get_stdout_buffer().empty();
                        const bool no_stderr_output =  get_stderr_buffer().empty();

                        if (no_stdout_output && no_stderr_output)
                        {
                            /* The builtin produced no output and is not inside of a pipeline. No need to fork or even output anything. */
                            if (g_log_forks)
                            {
                                // This one is very wordy
                                //printf("fork #-: Skipping fork due to no output for internal builtin for '%ls'\n", p->argv0());
                            }
                            fork_was_skipped = true;
                        }
                        else if (no_stderr_output && stdout_is_to_buffer)
                        {
                            /* The builtin produced no stderr, and its stdout is going to an internal buffer. There is no need to fork. This helps out the performance quite a bit in complex completion code. */
                            if (g_log_forks)
                            {
                                printf("fork #-: Skipping fork due to buffered output for internal builtin for '%ls'\n", p->argv0());
                            }

                            CAST_INIT(io_buffer_t *, io_buffer, stdout_io.get());
                            const std::string res = wcs2string(get_stdout_buffer());
                            io_buffer->out_buffer_append(res.data(), res.size());
                            fork_was_skipped = true;
                        }
                        else if (stdout_io.get() == NULL && stderr_io.get() == NULL)
                        {
                            /* We are writing to normal stdout and stderr. Just do it - no need to fork. */
                            if (g_log_forks)
                            {
                                printf("fork #-: Skipping fork due to ordinary output for internal builtin for '%ls'\n", p->argv0());
                            }
                            const wcstring &out = get_stdout_buffer(), &err = get_stderr_buffer();
                            const std::string outbuff = wcs2string(out);
                            const std::string errbuff = wcs2string(err);
                            bool builtin_io_done = do_builtin_io(outbuff.data(), outbuff.size(), errbuff.data(), errbuff.size());
                            if (! builtin_io_done)
                            {
                                show_stackframe();
                            }
                            fork_was_skipped = true;
                        }
                    }
                }
                

                if (fork_was_skipped)
                {
                    p->completed=1;
                    if (p->next == 0)
                    {
                        debug(3, L"Set status of %ls to %d using short circuit", j->command_wcstr(), p->status);

                        int status = p->status;
                        proc_set_last_status(job_get_flag(j, JOB_NEGATE)?(!status):status);
                    }
                }
                else
                {


                    /* Ok, unfortunately, we have to do a real fork. Bummer. We work hard to make sure we don't have to wait for all our threads to exit, by arranging things so that we don't have to allocate memory or do anything except system calls in the child. */

                    /* Get the strings we'll write before we fork (since they call malloc) */
                    const wcstring &out = get_stdout_buffer(), &err = get_stderr_buffer();

                    /* These strings may contain embedded nulls, so don't treat them as C strings */
                    const std::string outbuff_str = wcs2string(out);
                    const char *outbuff = outbuff_str.data();
                    size_t outbuff_len = outbuff_str.size();

                    const std::string errbuff_str = wcs2string(err);
                    const char *errbuff = errbuff_str.data();
                    size_t errbuff_len = errbuff_str.size();

                    fflush(stdout);
                    fflush(stderr);
                    if (g_log_forks)
                    {
                        printf("fork #%d: Executing fork for internal builtin for '%ls'\n", g_fork_count, p->argv0());
                        io_print(process_net_io_chain);
                    }
                    pid = execute_fork(false);
                    if (pid == 0)
                    {
                        /*
                          This is the child process. Setup redirections,
                          print correct output to stdout and stderr, and
                          then exit.
                        */
                        p->pid = getpid();
                        setup_child_process(j, p, process_net_io_chain);
                        do_builtin_io(outbuff, outbuff_len, errbuff, errbuff_len);
                        exit_without_destructors(p->status);
                    }
                    else
                    {
                        /*
                           This is the parent process. Store away
                           information on the child, and possibly give
                           it control over the terminal.
                        */
                        p->pid = pid;

                        set_child_group(j, p, 0);
                    }
                }

                break;
            }

            case EXTERNAL:
            {
                /* Get argv and envv before we fork */
                null_terminated_array_t<char> argv_array;
                convert_wide_array_to_narrow(p->get_argv_array(), &argv_array);

                /* Ensure that stdin is blocking before we hand it off (see issue #176). It's a little strange that we only do this with stdin and not with stdout or stderr. However in practice, setting or clearing O_NONBLOCK on stdin also sets it for the other two fds, presumably because they refer to the same underlying file (/dev/tty?) */
                make_fd_blocking(STDIN_FILENO);

                const char * const *argv = argv_array.get();
                const char * const *envv = env_export_arr(false);

                std::string actual_cmd_str = wcs2string(p->actual_cmd);
                const char *actual_cmd = actual_cmd_str.c_str();

                const wchar_t *reader_current_filename(void);
                if (g_log_forks)
                {
                    const wchar_t *file = reader_current_filename();
                    const wchar_t *func = parser_t::principal_parser().is_function();
                    printf("fork #%d: forking for '%s' in '%ls:%ls'\n", g_fork_count, actual_cmd, file ? file : L"", func ? func : L"?");

                    fprintf(stderr, "IO chain for %s:\n", actual_cmd);
                    io_print(process_net_io_chain);
                }

#if FISH_USE_POSIX_SPAWN
                /* Prefer to use posix_spawn, since it's faster on some systems like OS X */
                bool use_posix_spawn = g_use_posix_spawn && can_use_posix_spawn_for_job(j, p);
                if (use_posix_spawn)
                {
                    /* Create posix spawn attributes and actions */
                    posix_spawnattr_t attr = posix_spawnattr_t();
                    posix_spawn_file_actions_t actions = posix_spawn_file_actions_t();
                    bool made_it = fork_actions_make_spawn_properties(&attr, &actions, j, p, process_net_io_chain);
                    if (made_it)
                    {
                        /* We successfully made the attributes and actions; actually call posix_spawn */
                        int spawn_ret = posix_spawn(&pid, actual_cmd, &actions, &attr, const_cast<char * const *>(argv), const_cast<char * const *>(envv));

                        /* This usleep can be used to test for various race conditions (https://github.com/fish-shell/fish-shell/issues/360) */
                        //usleep(10000);

                        if (spawn_ret != 0)
                        {
                            safe_report_exec_error(spawn_ret, actual_cmd, argv, envv);
                            /* Make sure our pid isn't set */
                            pid = 0;
                        }

                        /* Clean up our actions */
                        posix_spawn_file_actions_destroy(&actions);
                        posix_spawnattr_destroy(&attr);
                    }

                    /* A 0 pid means we failed to posix_spawn. Since we have no pid, we'll never get told when it's exited, so we have to mark the process as failed. */
                    if (pid == 0)
                    {
                        job_mark_process_as_failed(j, p);
                        exec_error = true;
                    }
                }
                else
#endif
                {
                    pid = execute_fork(false);
                    if (pid == 0)
                    {
                        /* This is the child process. */
                        p->pid = getpid();
                        setup_child_process(j, p, process_net_io_chain);
                        safe_launch_process(p, actual_cmd, argv, envv);

                        /*
                          safe_launch_process _never_ returns...
                        */
                    }
                    else if (pid < 0)
                    {
                        job_mark_process_as_failed(j, p);
                        exec_error = true;
                    }
                }


                /*
                   This is the parent process. Store away
                   information on the child, and possibly fice
                   it control over the terminal.
                */
                p->pid = pid;

                set_child_group(j, p, 0);

                break;
            }

        }

        if (p->type == INTERNAL_BUILTIN)
            builtin_pop_io(parser);

        /*
           Close the pipe the current process uses to read from the
           previous process_t
        */
        if (pipe_current_read >= 0)
        {
            exec_close(pipe_current_read);
            pipe_current_read = -1;
        }

        /* Close the write end too, since the curent child subprocess already has a copy of it. */
        if (pipe_current_write >= 0)
        {
            exec_close(pipe_current_write);
            pipe_current_write = -1;
        }
    }

    /* Clean up any file descriptors we left open */
    if (pipe_current_read >= 0)
        exec_close(pipe_current_read);
    if (pipe_current_write >= 0)
        exec_close(pipe_current_write);
    if (pipe_next_read >= 0)
        exec_close(pipe_next_read);

    /* The keepalive process is no longer needed, so we terminate it with extreme prejudice */
    if (needs_keepalive)
    {
        kill(keepalive.pid, SIGKILL);
    }

    signal_unblock();

    debug(3, L"Job is constructed");

    job_set_flag(j, JOB_CONSTRUCTED, 1);

    if (!job_get_flag(j, JOB_FOREGROUND))
    {
        proc_last_bg_pid = j->pgid;
    }

    if (! exec_error)
    {
        job_continue(j, false);
    }
    else
    {
        /* Mark the errored job as not in the foreground. I can't fully justify whether this is the right place, but it prevents sanity_lose from complaining. */
        job_set_flag(j, JOB_FOREGROUND, 0);
    }

}


static int exec_subshell_internal(const wcstring &cmd, wcstring_list_t *lst, bool apply_exit_status)
{
    ASSERT_IS_MAIN_THREAD();
    int prev_subshell = is_subshell;
    const int prev_status = proc_get_last_status();
    char sep=0;

    const env_var_t ifs = env_get_string(L"IFS");

    if (! ifs.missing_or_empty())
    {
        if (ifs.at(0) < 128)
        {
            sep = '\n';//ifs[0];
        }
        else
        {
            sep = 0;
            debug(0, L"Warning - invalid command substitution separator '%lc'. Please change the first character of IFS", ifs[0]);
        }

    }

    is_subshell=1;


    int subcommand_status = -1; //assume the worst

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may be null
    const shared_ptr<io_buffer_t> io_buffer(io_buffer_t::create(0));
    if (io_buffer.get() != NULL)
    {
        parser_t &parser = parser_t::principal_parser();
        if (parser.eval(cmd, io_chain_t(io_buffer), SUBST) == 0)
        {
            subcommand_status = proc_get_last_status();
        }

        io_buffer->read();
    }

    // If the caller asked us to preserve the exit status, restore the old status
    // Otherwise set the status of the subcommand
    proc_set_last_status(apply_exit_status ? subcommand_status : prev_status);


    is_subshell = prev_subshell;

    if (lst != NULL && io_buffer.get() != NULL)
    {
        const char *begin = io_buffer->out_buffer_ptr();
        const char *end = begin + io_buffer->out_buffer_size();
        const char *cursor = begin;
        while (cursor < end)
        {
            // Look for the next separator
            const char *stop = (const char *)memchr(cursor, sep, end - cursor);
            const bool hit_separator = (stop != NULL);
            if (! hit_separator)
            {
                // If it's not found, just use the end
                stop = end;
            }
            // Stop now points at the first character we do not want to copy
            const wcstring wc = str2wcstring(cursor, stop - cursor);
            lst->push_back(wc);

            // If we hit a separator, skip over it; otherwise we're at the end
            cursor = stop + (hit_separator ? 1 : 0);
        }
    }

    return subcommand_status;
}

int exec_subshell(const wcstring &cmd, std::vector<wcstring> &outputs, bool apply_exit_status)
{
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, &outputs, apply_exit_status);
}

int exec_subshell(const wcstring &cmd, bool apply_exit_status)
{
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, NULL, apply_exit_status);
}
