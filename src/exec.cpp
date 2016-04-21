/** \file exec.c
  Functions for executing a program.

  Some of the code in this file is based on code from the Glibc
  manual, though the changes performed have been massive.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <vector>
#include <algorithm>
#ifdef HAVE_SPAWN_H
#include <spawn.h>
#endif
#include <wctype.h>
#include <map>
#include <string>
#include <memory>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <stdbool.h>

#include "fallback.h"  // IWYU pragma: keep
#include "postfork.h"
#include "common.h"
#include "wutil.h"  // IWYU pragma: keep
#include "proc.h"
#include "exec.h"
#include "parser.h"
#include "builtin.h"
#include "function.h"
#include "env.h"
#include "signal.h"
#include "io.h"
#include "parse_tree.h"
#include "reader.h"

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
    
    // Pipes ought to be cloexec
    // Pipes are dup2'd the corresponding fds; the resulting fds are not cloexec
    set_cloexec(fd[0]);
    set_cloexec(fd[1]);
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
        const io_data_t *io = io_chain.at(idx).get();
        if (redirection_is_to_real_file(io))
        {
            result = true;
            break;
        }
    }
    return result;
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
   This function is similar to launch_process, except it is not called after a
   fork (i.e. it only calls exec) and therefore it can allocate memory.
*/
static void launch_process_nofork(process_t *p)
{
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

    null_terminated_array_t<char> argv_array;
    convert_wide_array_to_narrow(p->get_argv_array(), &argv_array);

    const char *const *envv = env_export_arr(false);
    char *actual_cmd = wcs2str(p->actual_cmd.c_str());

    /* Ensure the terminal modes are what they were before we changed them. */
    restore_term_mode();
    /* Bounce to launch_process. This never returns. */
    safe_launch_process(p, actual_cmd, argv_array.get(), envv);
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

   \return true on success, false on failure. Returns the output chain and opened_fds by reference
*/
static bool io_transmogrify(const io_chain_t &in_chain, io_chain_t *out_chain, std::vector<int> *out_opened_fds)
{
    ASSERT_IS_MAIN_THREAD();
    assert(out_chain != NULL && out_opened_fds != NULL);
    assert(out_chain->empty());

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
                out.reset(new io_fd_t(in->fd, fd, false));

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
        out_chain->swap(result_chain);
        out_opened_fds->swap(opened_fds);
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

   \param def the code to evaluate, or the empty string if none
   \param node_offset the offset of the node to evalute, or NODE_OFFSET_INVALID
   \param block_type the type of block to push on evaluation
   \param io the io redirections to be performed on this block
*/

static void internal_exec_helper(parser_t &parser,
                                 const wcstring &def,
                                 node_offset_t node_offset,
                                 enum block_type_t block_type,
                                 const io_chain_t &ios)
{
    // If we have a valid node offset, then we must not have a string to execute
    assert(node_offset == NODE_OFFSET_INVALID || def.empty());

    io_chain_t morphed_chain;
    std::vector<int> opened_fds;
    bool transmorgrified = io_transmogrify(ios, &morphed_chain, &opened_fds);

    /*
      Did the transmogrification fail - if so, set error status and return
    */
    if (! transmorgrified)
    {
        proc_set_last_status(STATUS_EXEC_FAIL);
        return;
    }

    signal_unblock();

    if (node_offset == NODE_OFFSET_INVALID)
    {
        parser.eval(def, morphed_chain, block_type);
    }
    else
    {
        parser.eval_block_node(node_offset, morphed_chain, block_type);
    }

    signal_block();

    morphed_chain.clear();
    io_cleanup_fds(opened_fds);
    job_reap(0);
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

    /* If fish was invoked with -n or --no-execute, then no_exec will be set and we do nothing. */
    if (no_exec)
    {
        return;
    }

    sigemptyset(&chldset);
    sigaddset(&chldset, SIGCHLD);

    debug(4, L"Exec job '%ls' with id %d", j->command_wcstr(), j->job_id);

    /* Verify that all IO_BUFFERs are output. We used to support a (single, hacked-in) magical input IO_BUFFER used by fish_pager, but now the claim is that there are no more clients and it is removed. This assertion double-checks that. */
    const io_chain_t all_ios = j->all_io_redirections();
    for (size_t idx = 0; idx < all_ios.size(); idx++)
    {
        const shared_ptr<io_data_t> &io = all_ios.at(idx);

        if ((io->io_mode == IO_BUFFER))
        {
            CAST_INIT(io_buffer_t *, io_buffer, io.get());
            assert(! io_buffer->is_input);
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
            /* decrement SHLVL as we're removing ourselves from the shell "stack" */
            const env_var_t shlvl_str = env_get_string(L"SHLVL", ENV_GLOBAL | ENV_EXPORT);
            wcstring nshlvl_str = L"0";
            if (!shlvl_str.missing())
            {
                wchar_t *end;
                long shlvl_i = wcstol(shlvl_str.c_str(), &end, 10);
                while (iswspace(*end)) ++end; /* skip trailing whitespace */
                if (shlvl_i > 0 && *end == '\0')
                {
                    nshlvl_str = to_string<long>(shlvl_i - 1);
                }
            }
            env_set(L"SHLVL", nshlvl_str.c_str(), ENV_GLOBAL | ENV_EXPORT);

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
        assert(0 && "This should be unreachable");
    }
    
    /* We may have block IOs that conflict with fd redirections. For example, we may have a command with a redireciton like <&3; we may also have chosen 3 as the fd for our pipe. Ensure we have no conflicts. */
    for (size_t i=0; i < all_ios.size(); i++)
    {
        io_data_t *io = all_ios.at(i).get();
        if (io->io_mode == IO_BUFFER)
        {
            CAST_INIT(io_buffer_t *, io_buffer, io);
            if (! io_buffer->avoid_conflicts_with_io_chain(all_ios))
            {
                /* We could not avoid conflicts, probably due to fd exhaustion. Mark an error. */
                exec_error = true;
                job_mark_process_as_failed(j, j->first_process);
                break;
            }
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

    if (job_get_flag(j, JOB_CONTROL) && ! exec_error)
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
    for (process_t *p=j->first_process; p != NULL && ! exec_error; p = p->next)
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

               echo "begin; $argv "\n" ;end <&3 3<&-" | source 3<&0

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


        /* Set up fds that will be used in the pipe. */
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

            /* Ensure our pipe fds not conflict with any fd redirections. E.g. if the process is like 'cat <&5' then fd 5 must not be used by the pipe. */
            if (! pipe_avoid_conflicts_with_io_chain(local_pipe, all_ios))
            {
                /* We failed. The pipes were closed for us. */
                wperror(L"dup");
                exec_error = true;
                job_mark_process_as_failed(j, p);
                break;
            }

            // This tells the redirection about the fds, but the redirection does not close them
            assert(local_pipe[0] >= 0);
            assert(local_pipe[1] >= 0);
            memcpy(pipe_write->pipe_fd, local_pipe, sizeof(int)*2);

            // Record our pipes
            // The fds should be negative to indicate that we aren't overwriting an fd we failed to close
            assert(pipe_current_write == -1);
            pipe_current_write = local_pipe[1];

            assert(pipe_next_read == -1);
            pipe_next_read = local_pipe[0];
        }

        // This is the IO buffer we use for storing the output of a block or function when it is in a pipeline
        shared_ptr<io_buffer_t> block_output_io_buffer;
        
        // This is the io_streams we pass to internal builtins
        std::auto_ptr<io_streams_t> builtin_io_streams;
        
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
                const wcstring func_name = p->argv0();
                wcstring def;
                bool function_exists = function_get_definition(func_name, &def);

                bool shadows = function_get_shadows(func_name);
                const std::map<wcstring,env_var_t> inherit_vars = function_get_inherit_vars(func_name);

                signal_block();

                if (! function_exists)
                {
                    debug(0, _(L"Unknown function '%ls'"), p->argv0());
                    break;
                }
                function_block_t *newv = new function_block_t(p, func_name, shadows);
                parser.push_block(newv);

                /*
                  setting variables might trigger an event
                  handler, hence we need to unblock
                  signals.
                */
                signal_unblock();
                function_prepare_environment(func_name, p->get_argv()+1, inherit_vars);
                signal_block();

                parser.forbid_function(func_name);

                if (p->next)
                {
                    // Be careful to handle failure, e.g. too many open fds
                    block_output_io_buffer.reset(io_buffer_t::create(STDOUT_FILENO, all_ios));
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
                    internal_exec_helper(parser, def, NODE_OFFSET_INVALID, TOP, process_net_io_chain);
                }

                parser.allow_function();
                parser.pop_block();

                break;
            }

            case INTERNAL_BLOCK_NODE:
            {
                if (p->next)
                {
                    block_output_io_buffer.reset(io_buffer_t::create(STDOUT_FILENO, all_ios));
                    if (block_output_io_buffer.get() == NULL)
                    {
                        /* We failed (e.g. no more fds could be created). */
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
                    internal_exec_helper(parser, wcstring(), p->internal_block_node, TOP, process_net_io_chain);
                }
                break;
            }

            case INTERNAL_BUILTIN:
            {
                int local_builtin_stdin = STDIN_FILENO;
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
                                /* Ignore user-supplied fd redirections from an fd other than the standard ones. e.g. in source <&3 don't actually read from fd 3, which is internal to fish. We still respect this redirection in that we pass it on as a block IO to the code that source runs, and therefore this is not an error. Non-user supplied fd redirections come about through transmogrification, and we need to respect those here. */
                                if (! in_fd->user_supplied || (in_fd->old_fd >= 0 && in_fd->old_fd < 3))
                                {
                                    local_builtin_stdin = in_fd->old_fd;
                                }
                                break;
                            }
                            case IO_PIPE:
                            {
                                CAST_INIT(const io_pipe_t *, in_pipe, in.get());
                                local_builtin_stdin = in_pipe->pipe_fd[0];
                                break;
                            }

                            case IO_FILE:
                            {
                                /* Do not set CLO_EXEC because child needs access */
                                CAST_INIT(const io_file_t *, in_file, in.get());
                                local_builtin_stdin=open(in_file->filename_cstr,
                                                   in_file->flags, OPEN_MASK);
                                if (local_builtin_stdin == -1)
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
                                local_builtin_stdin = -1;

                                break;
                            }

                            default:
                            {
                                local_builtin_stdin=-1;
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
                    local_builtin_stdin = pipe_read->pipe_fd[0];
                }

                if (local_builtin_stdin == -1)
                {
                    exec_error = true;
                    break;
                }
                else
                {
                    // Determine if we have a "direct" redirection for stdin
                    bool stdin_is_directly_redirected;
                    if (p != j->first_process)
                    {
                        // We must have a pipe
                        stdin_is_directly_redirected = true;
                    }
                    else
                    {
                        // We are not a pipe. Check if there is a redirection local to the process that's not IO_CLOSE
                        const shared_ptr<const io_data_t> stdin_io = io_chain_get(p->io_chain(), STDIN_FILENO);
                        stdin_is_directly_redirected = stdin_io && stdin_io->io_mode != IO_CLOSE;
                    }
                    
                    builtin_io_streams.reset(new io_streams_t());
                    builtin_io_streams->stdin_fd = local_builtin_stdin;
                    builtin_io_streams->out_is_redirected = has_fd(process_net_io_chain, STDOUT_FILENO);
                    builtin_io_streams->err_is_redirected = has_fd(process_net_io_chain, STDERR_FILENO);
                    builtin_io_streams->stdin_is_directly_redirected = stdin_is_directly_redirected;
                    builtin_io_streams->io_chain = &process_net_io_chain;

                    
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

                    const int fg = job_get_flag(j, JOB_FOREGROUND);
                    job_set_flag(j, JOB_FOREGROUND, 0);

                    signal_unblock();
                    
                    p->status = builtin_run(parser, p->get_argv(), *builtin_io_streams);

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
                    exec_close(local_builtin_stdin);
                }
                break;
            }

            case EXTERNAL:
                /* External commands are handled in the next switch statement below */
                break;

            case INTERNAL_EXEC:
                /* We should have handled exec up above */
                assert(0 && "INTERNAL_EXEC process found in pipeline, where it should never be. Aborting.");
                break;
        }

        if (exec_error)
        {
            break;
        }

        switch (p->type)
        {

            case INTERNAL_BLOCK_NODE:
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
                    if (p->next == NULL)
                    {
                        proc_set_last_status(job_get_flag(j, JOB_NEGATE)?(!status):status);
                    }
                    p->completed = 1;
                    break;
                }

                // Here we must have a non-NULL block_output_io_buffer
                assert(block_output_io_buffer.get() != NULL);
                process_net_io_chain.remove(block_output_io_buffer);

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
                
                assert(builtin_io_streams.get() != NULL);
                const wcstring &stdout_buffer = builtin_io_streams->out.buffer();
                const wcstring &stderr_buffer = builtin_io_streams->err.buffer();

                /* If we are outputting to a file, we have to actually do it, even if we have no output, so that we can truncate the file. Does not apply to /dev/null. */
                bool must_fork = redirection_is_to_real_file(stdout_io.get()) || redirection_is_to_real_file(stderr_io.get());
                if (! must_fork)
                {
                    if (p->next == NULL)
                    {
                        const bool stdout_is_to_buffer = stdout_io && stdout_io->io_mode == IO_BUFFER;
                        const bool no_stdout_output =  stdout_buffer.empty();
                        const bool no_stderr_output =  stderr_buffer.empty();

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
                            const std::string res = wcs2string(builtin_io_streams->out.buffer());
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
                            const std::string outbuff = wcs2string(stdout_buffer);
                            const std::string errbuff = wcs2string(stderr_buffer);
                            bool builtin_io_done = do_builtin_io(outbuff.data(), outbuff.size(), errbuff.data(), errbuff.size());
                            if (! builtin_io_done && errno != EPIPE)
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

                    /* These strings may contain embedded nulls, so don't treat them as C strings */
                    const std::string outbuff_str = wcs2string(stdout_buffer);
                    const char *outbuff = outbuff_str.data();
                    size_t outbuff_len = outbuff_str.size();

                    const std::string errbuff_str = wcs2string(stderr_buffer);
                    const char *errbuff = errbuff_str.data();
                    size_t errbuff_len = errbuff_str.size();

                    fflush(stdout);
                    fflush(stderr);
                    if (g_log_forks)
                    {
                        printf("fork #%d: Executing fork for internal builtin for '%ls'\n", g_fork_count, p->argv0());
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
                    printf("fork #%d: forking for '%s' in '%ls'\n", g_fork_count, actual_cmd, file ? file : L"");
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
                        assert(0 && "safe_launch_process should not have returned");
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

            case INTERNAL_EXEC:
            {
                /* We should have handled exec up above */
                assert(0 && "INTERNAL_EXEC process found in pipeline, where it should never be. Aborting.");
                break;
            }
        }

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
    bool split_output=false;

    //fprintf(stderr, "subcmd %ls\n", cmd.c_str());

    const env_var_t ifs = env_get_string(L"IFS");

    if (! ifs.missing_or_empty())
    {
        split_output=true;
    }

    is_subshell=1;


    int subcommand_status = -1; //assume the worst

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may be null
    const shared_ptr<io_buffer_t> io_buffer(io_buffer_t::create(STDOUT_FILENO, io_chain_t()));
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
        if (split_output)
        {
            const char *cursor = begin;
            while (cursor < end)
            {
                // Look for the next separator
                const char *stop = (const char *)memchr(cursor, '\n', end - cursor);
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
        else
        {
            // we're not splitting output, but we still want to trim off a trailing newline
            if (end != begin && end[-1] == '\n')
            {
                --end;
            }
            const wcstring wc = str2wcstring(begin, end - begin);
            lst->push_back(wc);
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
