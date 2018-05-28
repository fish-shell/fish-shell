// Functions for executing a program.
//
// Some of the code in this file is based on code from the Glibc manual, though the changes
// performed have been massive.
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <signal.h>
#ifdef HAVE_SPAWN_H
#include <spawn.h>
#endif
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "io.h"
#include "parse_tree.h"
#include "parser.h"
#include "postfork.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

/// File descriptor redirection error message.
#define FD_ERROR _(L"An error occurred while redirecting file descriptor %d")

/// File descriptor redirection error message.
#define WRITE_ERROR _(L"An error occurred while writing output")

/// File redirection error message.
#define FILE_ERROR _(L"An error occurred while redirecting file '%s'")

/// Base open mode to pass to calls to open.
#define OPEN_MASK 0666

/// Called in a forked child.
static void exec_write_and_exit(int fd, const char *buff, size_t count, int status) {
    if (write_loop(fd, buff, count) == -1) {
        debug(0, WRITE_ERROR);
        wperror(L"write");
        exit_without_destructors(status);
    }
    exit_without_destructors(status);
}

void exec_close(int fd) {
    ASSERT_IS_MAIN_THREAD();

    // This may be called in a child of fork(), so don't allocate memory.
    if (fd < 0) {
        debug(0, L"Called close on invalid file descriptor ");
        return;
    }

    while (close(fd) == -1) {
        if (errno != EINTR) {
            debug(1, FD_ERROR, fd);
            wperror(L"close");
            break;
        }
    }
}

int exec_pipe(int fd[2]) {
    ASSERT_IS_MAIN_THREAD();

    int res;
    while ((res = pipe(fd))) {
        if (errno != EINTR) {
            return res;  // caller will call wperror
        }
    }

    debug(4, L"Created pipe using fds %d and %d", fd[0], fd[1]);

    // Pipes ought to be cloexec. Pipes are dup2'd the corresponding fds; the resulting fds are not
    // cloexec.
    set_cloexec(fd[0]);
    set_cloexec(fd[1]);
    return res;
}

/// Returns true if the redirection is a file redirection to a file other than /dev/null.
static bool redirection_is_to_real_file(const io_data_t *io) {
    bool result = false;
    if (io != NULL && io->io_mode == IO_FILE) {
        // It's a file redirection. Compare the path to /dev/null.
        const io_file_t *io_file = static_cast<const io_file_t *>(io);
        const char *path = io_file->filename_cstr;
        if (strcmp(path, "/dev/null") != 0) {
            // It's not /dev/null.
            result = true;
        }
    }
    return result;
}

static bool chain_contains_redirection_to_real_file(const io_chain_t &io_chain) {
    bool result = false;
    for (size_t idx = 0; idx < io_chain.size(); idx++) {
        const io_data_t *io = io_chain.at(idx).get();
        if (redirection_is_to_real_file(io)) {
            result = true;
            break;
        }
    }
    return result;
}

/// Returns the interpreter for the specified script. Returns NULL if file is not a script with a
/// shebang.
char *get_interpreter(const char *command, char *interpreter, size_t buff_size) {
    // OK to not use CLO_EXEC here because this is only called after fork.
    int fd = open(command, O_RDONLY);
    if (fd >= 0) {
        size_t idx = 0;
        while (idx + 1 < buff_size) {
            char ch;
            ssize_t amt = read(fd, &ch, sizeof ch);
            if (amt <= 0) break;
            if (ch == '\n') break;
            interpreter[idx++] = ch;
        }
        interpreter[idx++] = '\0';
        close(fd);
    }

    if (strncmp(interpreter, "#! /", 4) == 0) {
        return interpreter + 3;
    } else if (strncmp(interpreter, "#!/", 3) == 0) {
        return interpreter + 2;
    }

    return NULL;
}

/// This function is executed by the child process created by a call to fork(). It should be called
/// after \c setup_child_process. It calls execve to replace the fish process image with the command
/// specified in \c p. It never returns. Called in a forked child! Do not allocate memory, etc.
static void safe_launch_process(process_t *p, const char *actual_cmd, const char *const *cargv,
                                const char *const *cenvv) {
    UNUSED(p);
    int err;
    //  debug( 1, L"exec '%ls'", p->argv[0] );

    // This function never returns, so we take certain liberties with constness.
    char *const *envv = const_cast<char *const *>(cenvv);
    char *const *argv = const_cast<char *const *>(cargv);

    execve(actual_cmd, argv, envv);
    err = errno;

    // Something went wrong with execve, check for a ":", and run /bin/sh if encountered. This is a
    // weird predecessor to the shebang that is still sometimes used since it is supported on
    // Windows. OK to not use CLO_EXEC here because this is called after fork and the file is
    // immediately closed.
    int fd = open(actual_cmd, O_RDONLY);
    if (fd >= 0) {
        char begin[1] = {0};
        ssize_t amt_read = read(fd, begin, 1);
        close(fd);

        if ((amt_read == 1) && (begin[0] == ':')) {
            // Relaunch it with /bin/sh. Don't allocate memory, so if you have more args than this,
            // update your silly script! Maybe this should be changed to be based on ARG_MAX
            // somehow.
            char sh_command[] = "/bin/sh";
            char *argv2[128];
            argv2[0] = sh_command;
            for (size_t i = 1; i < sizeof argv2 / sizeof *argv2; i++) {
                argv2[i] = argv[i - 1];
                if (argv2[i] == NULL) break;
            }

            execve(sh_command, argv2, envv);
        }
    }

    errno = err;
    safe_report_exec_error(errno, actual_cmd, argv, envv);
    exit_without_destructors(STATUS_EXEC_FAIL);
}

/// This function is similar to launch_process, except it is not called after a fork (i.e. it only
/// calls exec) and therefore it can allocate memory.
static void launch_process_nofork(process_t *p) {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

    null_terminated_array_t<char> argv_array;
    convert_wide_array_to_narrow(p->get_argv_array(), &argv_array);

    const char *const *envv = env_export_arr();
    char *actual_cmd = wcs2str(p->actual_cmd);

    // Ensure the terminal modes are what they were before we changed them.
    restore_term_mode();
    // Bounce to launch_process. This never returns.
    safe_launch_process(p, actual_cmd, argv_array.get(), envv);
}

/// Check if the IO redirection chains contains redirections for the specified file descriptor.
static int has_fd(const io_chain_t &d, int fd) { return io_chain_get(d, fd).get() != NULL; }

/// Close a list of fds.
static void io_cleanup_fds(const std::vector<int> &opened_fds) {
    std::for_each(opened_fds.begin(), opened_fds.end(), close);
}

/// Make a copy of the specified io redirection chain, but change file redirection into fd
/// redirection. This makes the redirection chain suitable for use as block-level io, since the file
/// won't be repeatedly reopened for every command in the block, which would reset the cursor
/// position.
///
/// \return true on success, false on failure. Returns the output chain and opened_fds by reference.
static bool io_transmogrify(const io_chain_t &in_chain, io_chain_t *out_chain,
                            std::vector<int> *out_opened_fds) {
    ASSERT_IS_MAIN_THREAD();
    assert(out_chain != NULL && out_opened_fds != NULL);
    assert(out_chain->empty());

    // Just to be clear what we do for an empty chain.
    if (in_chain.empty()) {
        return true;
    }

    bool success = true;

    // Make our chain of redirections.
    io_chain_t result_chain;

    // In the event we can't finish transmorgrifying, we'll have to close all the files we opened.
    std::vector<int> opened_fds;

    for (size_t idx = 0; idx < in_chain.size(); idx++) {
        const shared_ptr<io_data_t> &in = in_chain.at(idx);
        shared_ptr<io_data_t> out;  // gets allocated via new

        switch (in->io_mode) {
            case IO_PIPE:
            case IO_FD:
            case IO_BUFFER:
            case IO_CLOSE: {
                // These redirections don't need transmogrification. They can be passed through.
                out = in;
                break;
            }
            case IO_FILE: {
                // Transmogrify file redirections.
                int fd;
                io_file_t *in_file = static_cast<io_file_t *>(in.get());
                if ((fd = open(in_file->filename_cstr, in_file->flags, OPEN_MASK)) == -1) {
                    debug(1, FILE_ERROR, in_file->filename_cstr);

                    wperror(L"open");
                    success = false;
                    break;
                }

                opened_fds.push_back(fd);
                out.reset(new io_fd_t(in->fd, fd, false));
                break;
            }
        }

        if (out.get() != NULL) result_chain.push_back(out);

        // Don't go any further if we failed.
        if (!success) {
            break;
        }
    }

    // Now either return success, or clean up.
    if (success) {
        *out_chain = std::move(result_chain);
        *out_opened_fds = std::move(opened_fds);
    } else {
        result_chain.clear();
        io_cleanup_fds(opened_fds);
    }
    return success;
}

/// Morph an io redirection chain into redirections suitable for passing to eval, call eval, and
/// clean up morphed redirections.
///
/// \param parsed_source the parsed source code containing the node to evaluate
/// \param node the node to evaluate
/// \param ios the io redirections to be performed on this block
template <typename T>
void internal_exec_helper(parser_t &parser, parsed_source_ref_t parsed_source, tnode_t<T> node,
                          const io_chain_t &ios) {
    assert(parsed_source && node && "exec_helper missing source or without node");

    io_chain_t morphed_chain;
    std::vector<int> opened_fds;
    bool transmorgrified = io_transmogrify(ios, &morphed_chain, &opened_fds);

    // Did the transmogrification fail - if so, set error status and return.
    if (!transmorgrified) {
        proc_set_last_status(STATUS_EXEC_FAIL);
        return;
    }

    parser.eval_node(parsed_source, node, morphed_chain, TOP);

    morphed_chain.clear();
    io_cleanup_fds(opened_fds);
    job_reap(0);
}

// Returns whether we can use posix spawn for a given process in a given job. Per
// https://github.com/fish-shell/fish-shell/issues/364 , error handling for file redirections is too
// difficult with posix_spawn, so in that case we use fork/exec.
//
// Furthermore, to avoid the race between the caller calling tcsetpgrp() and the client checking the
// foreground process group, we don't use posix_spawn if we're going to foreground the process. (If
// we use fork(), we can call tcsetpgrp after the fork, before the exec, and avoid the race).
static bool can_use_posix_spawn_for_job(const job_t *job, const process_t *process) {
    if (job->get_flag(JOB_CONTROL)) {  //!OCLINT(collapsible if statements)
        // We are going to use job control; therefore when we launch this job it will get its own
        // process group ID. But will it be foregrounded?
        if (job->get_flag(JOB_TERMINAL) && job->get_flag(JOB_FOREGROUND)) {
            // It will be foregrounded, so we will call tcsetpgrp(), therefore do not use
            // posix_spawn.
            return false;
        }
    }

    // Now see if we have a redirection involving a file. The only one we allow is /dev/null, which
    // we assume will not fail.
    bool result = true;
    if (chain_contains_redirection_to_real_file(job->block_io_chain()) ||
        chain_contains_redirection_to_real_file(process->io_chain())) {
        result = false;
    }
    return result;
}

void internal_exec(job_t *j, const io_chain_t &&all_ios) {
    // Do a regular launch -  but without forking first...

    // setup_child_process makes sure signals are properly set up.

    // PCA This is for handling exec. Passing all_ios here matches what fish 2.0.0 and 1.x did.
    // It's known to be wrong - for example, it means that redirections bound for subsequent
    // commands in the pipeline will apply to exec. However, using exec in a pipeline doesn't
    // really make sense, so I'm not trying to fix it here.
    if (!setup_child_process(0, all_ios)) {
        // Decrement SHLVL as we're removing ourselves from the shell "stack".
        auto shlvl_var = env_get(L"SHLVL", ENV_GLOBAL | ENV_EXPORT);
        wcstring shlvl_str = L"0";
        if (shlvl_var) {
            long shlvl = fish_wcstol(shlvl_var->as_string().c_str());
            if (!errno && shlvl > 0) {
                shlvl_str = to_string<long>(shlvl - 1);
            }
        }
        env_set_one(L"SHLVL", ENV_GLOBAL | ENV_EXPORT, shlvl_str);

        // launch_process _never_ returns.
        launch_process_nofork(j->processes.front().get());
    } else {
        j->set_flag(JOB_CONSTRUCTED, true);
        j->processes.front()->completed = 1;
        return;
    }
}

/// Execute an internal builtin. Given a parser, a job within that parser, and a process within that
/// job corresponding to a builtin, execute the builtin with the given streams. If pipe_read is set,
/// assign stdin to it; otherwise infer stdin from the IO chain.
/// return true on success, false if there is an exec error.
static bool exec_internal_builtin_proc(parser_t &parser, job_t *j, process_t *p,
                                       const io_pipe_t *pipe_read, const io_chain_t &proc_io_chain,
                                       io_streams_t &streams) {
    assert(p->type == INTERNAL_BUILTIN && "Process must be a builtin");
    int local_builtin_stdin = STDIN_FILENO;
    bool close_stdin = false;

    // If this is the first process, check the io redirections and see where we should
    // be reading from.
    if (pipe_read) {
        local_builtin_stdin = pipe_read->pipe_fd[0];
    } else if (const auto in = proc_io_chain.get_io_for_fd(STDIN_FILENO)) {
        switch (in->io_mode) {
            case IO_FD: {
                const io_fd_t *in_fd = static_cast<const io_fd_t *>(in.get());
                // Ignore user-supplied fd redirections from an fd other than the
                // standard ones. e.g. in source <&3 don't actually read from fd 3,
                // which is internal to fish. We still respect this redirection in
                // that we pass it on as a block IO to the code that source runs,
                // and therefore this is not an error. Non-user supplied fd
                // redirections come about through transmogrification, and we need
                // to respect those here.
                if (!in_fd->user_supplied || (in_fd->old_fd >= 0 && in_fd->old_fd < 3)) {
                    local_builtin_stdin = in_fd->old_fd;
                }
                break;
            }
            case IO_PIPE: {
                const io_pipe_t *in_pipe = static_cast<const io_pipe_t *>(in.get());
                local_builtin_stdin = in_pipe->pipe_fd[0];
                break;
            }
            case IO_FILE: {
                // Do not set CLO_EXEC because child needs access.
                const io_file_t *in_file = static_cast<const io_file_t *>(in.get());
                local_builtin_stdin = open(in_file->filename_cstr, in_file->flags, OPEN_MASK);
                if (local_builtin_stdin == -1) {
                    debug(1, FILE_ERROR, in_file->filename_cstr);
                    wperror(L"open");
                } else {
                    close_stdin = true;
                }

                break;
            }
            case IO_CLOSE: {
                // FIXME: When requesting that stdin be closed, we really don't do
                // anything. How should this be handled?
                local_builtin_stdin = -1;

                break;
            }
            default: {
                local_builtin_stdin = -1;
                debug(1, _(L"Unknown input redirection type %d"), in->io_mode);
                break;
            }
        }
    }

    if (local_builtin_stdin == -1) return false;

    // Determine if we have a "direct" redirection for stdin.
    bool stdin_is_directly_redirected;
    if (!p->is_first_in_job) {
        // We must have a pipe
        stdin_is_directly_redirected = true;
    } else {
        // We are not a pipe. Check if there is a redirection local to the process
        // that's not IO_CLOSE.
        const shared_ptr<const io_data_t> stdin_io = io_chain_get(p->io_chain(), STDIN_FILENO);
        stdin_is_directly_redirected = stdin_io && stdin_io->io_mode != IO_CLOSE;
    }

    streams.stdin_fd = local_builtin_stdin;
    streams.out_is_redirected = has_fd(proc_io_chain, STDOUT_FILENO);
    streams.err_is_redirected = has_fd(proc_io_chain, STDERR_FILENO);
    streams.stdin_is_directly_redirected = stdin_is_directly_redirected;
    streams.io_chain = &proc_io_chain;

    // Since this may be the foreground job, and since a builtin may execute another
    // foreground job, we need to pretend to suspend this job while running the
    // builtin, in order to avoid a situation where two jobs are running at once.
    //
    // The reason this is done here, and not by the relevant builtins, is that this
    // way, the builtin does not need to know what job it is part of. It could
    // probably figure that out by walking the job list, but it seems more robust to
    // make exec handle things.
    const int fg = j->get_flag(JOB_FOREGROUND);
    j->set_flag(JOB_FOREGROUND, false);

    // Note this call may block for a long time, while the builtin performs I/O.
    p->status = builtin_run(parser, p->get_argv(), streams);

    // Restore the fg flag, which is temporarily set to false during builtin
    // execution so as not to confuse some job-handling builtins.
    j->set_flag(JOB_FOREGROUND, fg);

    // If stdin has been redirected, close the redirection stream.
    if (close_stdin) {
        exec_close(local_builtin_stdin);
    }
    return true;  // "success"
}

void on_process_created(job_t *j, pid_t child_pid) {
    // We only need to do this the first time a child is forked/spawned
    if (j->pgid != -2) {
        return;
    }

    if (j->get_flag(JOB_CONTROL)) {
        j->pgid = child_pid;
    } else {
        j->pgid = getpgrp();
    }
}

void exec_job(parser_t &parser, job_t *j) {
    pid_t pid = 0;

    // Set to true if something goes wrong while exec:ing the job, in which case the cleanup code
    // will kick in.
    bool exec_error = false;
    bool needs_keepalive = false;
    process_t keepalive;

    CHECK(j, );
    CHECK_BLOCK();

    // If fish was invoked with -n or --no-execute, then no_exec will be set and we do nothing.
    if (no_exec) {
        return;
    }

    debug(4, L"Exec job '%ls' with id %d", j->command_wcstr(), j->job_id);

    // Verify that all IO_BUFFERs are output. We used to support a (single, hacked-in) magical input
    // IO_BUFFER used by fish_pager, but now the claim is that there are no more clients and it is
    // removed. This assertion double-checks that.
    size_t stdout_read_limit = 0;
    const io_chain_t all_ios = j->all_io_redirections();
    for (size_t idx = 0; idx < all_ios.size(); idx++) {
        const shared_ptr<io_data_t> &io = all_ios.at(idx);

        if ((io->io_mode == IO_BUFFER)) {
            io_buffer_t *io_buffer = static_cast<io_buffer_t *>(io.get());
            assert(!io_buffer->is_input);
            stdout_read_limit = io_buffer->get_buffer_limit();
        }
    }

    if (j->processes.front()->type == INTERNAL_EXEC) {
        internal_exec(j, std::move(all_ios));
        DIE("this should be unreachable");
    }

    // We may have block IOs that conflict with fd redirections. For example, we may have a command
    // with a redireciton like <&3; we may also have chosen 3 as the fd for our pipe. Ensure we have
    // no conflicts.
    for (size_t i = 0; i < all_ios.size(); i++) {
        io_data_t *io = all_ios.at(i).get();
        if (io->io_mode == IO_BUFFER) {
            io_buffer_t *io_buffer = static_cast<io_buffer_t *>(io);
            if (!io_buffer->avoid_conflicts_with_io_chain(all_ios)) {
                // We could not avoid conflicts, probably due to fd exhaustion. Mark an error.
                exec_error = true;
                job_mark_process_as_failed(j, j->processes.front().get());
                break;
            }
        }
    }

    // See if we need to create a group keepalive process. This is a process that we create to make
    // sure that the process group doesn't die accidentally, and is often needed when a
    // builtin/block/function is inside a pipeline, since that usually means we have to wait for one
    // program to exit before continuing in the pipeline, causing the group leader to exit.
    if (j->get_flag(JOB_CONTROL) && !exec_error) {
        for (const process_ptr_t &p : j->processes) {
            if (p->type != EXTERNAL && (!p->is_last_in_job || !p->is_first_in_job)) {
                needs_keepalive = true;
                break;
            }
            // When running under WSL, create a keepalive process unconditionally if our first process is external.
            // This is because WSL does not permit joining the pgrp of an exited process.
            // (see https://github.com/Microsoft/WSL/issues/2786), also fish PR #4676
            if (is_windows_subsystem_for_linux() && j->processes.front()->type == EXTERNAL
                    && !p->is_first_in_job) { //but not if it's the only process
                needs_keepalive = true;
                break;
            }
        }
    }

    if (needs_keepalive) {
        // Call fork. No need to wait for threads since our use is confined and simple.
        pid_t parent_pid = getpid();
        keepalive.pid = execute_fork(false);
        if (keepalive.pid == 0) {
            // Child
            keepalive.pid = getpid();
            child_set_group(j, &keepalive);
            run_as_keepalive(parent_pid);
            exit_without_destructors(0);
        } else {
            // Parent
            debug(2, L"Fork #%d, pid %d: keepalive fork for '%ls'", g_fork_count, keepalive.pid,
                  j->command_wcstr());
            on_process_created(j, keepalive.pid);
            set_child_group(j, keepalive.pid);
            maybe_assign_terminal(j);
        }
    }

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
    // We are careful to set these to -1 when closed, so if we exit the loop abruptly, we can still
    // close them.
    int pipe_current_read = -1, pipe_current_write = -1, pipe_next_read = -1;
    for (std::unique_ptr<process_t> &unique_p : j->processes) {
        if (exec_error) {
            break;
        }
        process_t *const p = unique_p.get();
        // The IO chain for this process. It starts with the block IO, then pipes, and then gets any
        // from the process.
        io_chain_t process_net_io_chain = j->block_io_chain();

        // "Consume" any pipe_next_read by making it current.
        assert(pipe_current_read == -1);
        pipe_current_read = pipe_next_read;
        pipe_next_read = -1;

        // See if we need a pipe.
        const bool pipes_to_next_command = !p->is_last_in_job;
        // Set to true if we end up forking for this process.
        bool child_forked = false;
        bool child_spawned = false;

        // The pipes the current process write to and read from. Unfortunately these can't be just
        // allocated on the stack, since j->io wants shared_ptr.
        //
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
        shared_ptr<io_pipe_t> pipe_write;
        shared_ptr<io_pipe_t> pipe_read;

        // Write pipe goes first.
        if (pipes_to_next_command) {
            pipe_write.reset(new io_pipe_t(p->pipe_write_fd, false));
            process_net_io_chain.push_back(pipe_write);
        }

        // The explicit IO redirections associated with the process.
        process_net_io_chain.append(p->io_chain());

        // Read pipe goes last.
        if (!p->is_first_in_job) {
            pipe_read.reset(new io_pipe_t(p->pipe_read_fd, true));
            // Record the current read in pipe_read.
            pipe_read->pipe_fd[0] = pipe_current_read;
            process_net_io_chain.push_back(pipe_read);
        }

        // This call is used so the global environment variable array is regenerated, if needed,
        // before the fork. That way, we avoid a lot of duplicate work where EVERY child would need
        // to generate it, since that result would not get written back to the parent. This call
        // could be safely removed, but it would result in slightly lower performance - at least on
        // uniprocessor systems.
        if (p->type == EXTERNAL) {
            // Apply universal barrier so we have the most recent uvar changes
            if (!get_proc_had_barrier()) {
                set_proc_had_barrier(true);
                env_universal_barrier();
            }
            env_export_arr();
        }

        // Set up fds that will be used in the pipe.
        if (pipes_to_next_command) {
            // debug( 1, L"%ls|%ls" , p->argv[0], p->next->argv[0]);
            int local_pipe[2] = {-1, -1};
            if (exec_pipe(local_pipe) == -1) {
                debug(1, PIPE_ERROR);
                wperror(L"pipe");
                exec_error = true;
                job_mark_process_as_failed(j, p);
                break;
            }

            // Ensure our pipe fds not conflict with any fd redirections. E.g. if the process is
            // like 'cat <&5' then fd 5 must not be used by the pipe.
            if (!pipe_avoid_conflicts_with_io_chain(local_pipe, all_ios)) {
                // We failed. The pipes were closed for us.
                wperror(L"dup");
                exec_error = true;
                job_mark_process_as_failed(j, p);
                break;
            }

            // This tells the redirection about the fds, but the redirection does not close them.
            assert(local_pipe[0] >= 0);
            assert(local_pipe[1] >= 0);
            memcpy(pipe_write->pipe_fd, local_pipe, sizeof(int) * 2);

            // Record our pipes. The fds should be negative to indicate that we aren't overwriting
            // an fd we failed to close.
            assert(pipe_current_write == -1);
            pipe_current_write = local_pipe[1];

            assert(pipe_next_read == -1);
            pipe_next_read = local_pipe[0];
        }

        // This is the IO buffer we use for storing the output of a block or function when it is in
        // a pipeline.
        shared_ptr<io_buffer_t> block_output_io_buffer;

        // This is the io_streams we pass to internal builtins.
        std::unique_ptr<io_streams_t> builtin_io_streams(new io_streams_t(stdout_read_limit));

        // We fork in several different places. Each time the same code must be executed, so unify
        // it all here.
        auto do_fork = [&j, &p, &pid, &exec_error, &process_net_io_chain,
                        &child_forked](bool drain_threads, const char *fork_type,
                                       std::function<void()> child_action) -> bool {
            pid = execute_fork(drain_threads);
            if (pid == 0) {
                // This is the child process. Setup redirections, print correct output to
                // stdout and stderr, and then exit.
                p->pid = getpid();
                child_set_group(j, p);
                setup_child_process(p, process_net_io_chain);
                child_action();
                DIE("Child process returned control to do_fork lambda!");
            }

            if (pid < 0) {
                debug(1, L"Failed to fork %s!\n", fork_type);
                job_mark_process_as_failed(j, p);
                exec_error = true;
                return false;
            }

            // This is the parent process. Store away information on the child, and
            // possibly give it control over the terminal.
            debug(2, L"Fork #%d, pid %d: %s for '%ls'", g_fork_count, pid, fork_type, p->argv0());
            child_forked = true;

            p->pid = pid;
            on_process_created(j, p->pid);
            set_child_group(j, p->pid);
            maybe_assign_terminal(j);

            return true;
        };

        // Helper routine executed by INTERNAL_FUNCTION and INTERNAL_BLOCK_NODE to make sure an
        // output buffer exists in case there is another command in the job chain that will be
        // reading from this command's output.
        auto verify_buffer_output = [&]() {
            if (!p->is_last_in_job) {
                // Be careful to handle failure, e.g. too many open fds.
                block_output_io_buffer = io_buffer_t::create(STDOUT_FILENO, all_ios);
                if (block_output_io_buffer.get() == NULL) {
                    exec_error = true;
                    job_mark_process_as_failed(j, p);
                } else {
                    // This looks sketchy, because we're adding this io buffer locally - they
                    // aren't in the process or job redirection list. Therefore select_try won't
                    // be able to read them. However we call block_output_io_buffer->read()
                    // below, which reads until EOF. So there's no need to select on this.
                    process_net_io_chain.push_back(block_output_io_buffer);
                }
            }
        };

        switch (p->type) {
            case INTERNAL_FUNCTION: {
                const wcstring func_name = p->argv0();
                auto props = function_get_properties(func_name);
                if (!props) {
                    debug(0, _(L"Unknown function '%ls'"), p->argv0());
                    break;
                }

                const std::map<wcstring, env_var_t> inherit_vars =
                    function_get_inherit_vars(func_name);

                function_block_t *fb =
                    parser.push_block<function_block_t>(p, func_name, props->shadow_scope);
                function_prepare_environment(func_name, p->get_argv() + 1, inherit_vars);
                parser.forbid_function(func_name);

                verify_buffer_output();

                if (!exec_error) {
                    internal_exec_helper(parser, props->parsed_source, props->body_node,
                                         process_net_io_chain);
                }

                parser.allow_function();
                parser.pop_block(fb);

                break;
            }

            case INTERNAL_BLOCK_NODE: {
                verify_buffer_output();

                if (!exec_error) {
                    assert(p->block_node_source && p->internal_block_node &&
                           "Process is missing node info");
                    internal_exec_helper(parser, p->block_node_source, p->internal_block_node,
                                         process_net_io_chain);
                }
                break;
            }

            case INTERNAL_BUILTIN: {
                if (!exec_internal_builtin_proc(parser, j, p, pipe_read.get(), process_net_io_chain,
                                                *builtin_io_streams)) {
                    exec_error = true;
                }
                break;
            }

            case EXTERNAL:
                // External commands are handled in the next switch statement below.
                break;

            case INTERNAL_EXEC:
                // We should have handled exec up above.
                DIE("INTERNAL_EXEC process found in pipeline, where it should never be. Aborting.");
                break;
        }

        if (exec_error) {
            break;
        }

        switch (p->type) {
            case INTERNAL_BLOCK_NODE:
            case INTERNAL_FUNCTION: {
                int status = proc_get_last_status();

                // Handle output from a block or function. This usually means do nothing, but in the
                // case of pipes, we have to buffer such io, since otherwise the internal pipe
                // buffer might overflow.
                if (!block_output_io_buffer.get()) {
                    // No buffer, so we exit directly. This means we have to manually set the exit
                    // status.
                    if (p->is_last_in_job) {
                        proc_set_last_status(j->get_flag(JOB_NEGATE) ? (!status) : status);
                    }
                    p->completed = 1;
                    break;
                }

                // Here we must have a non-NULL block_output_io_buffer.
                assert(block_output_io_buffer.get() != NULL);
                process_net_io_chain.remove(block_output_io_buffer);

                block_output_io_buffer->read();

                const char *buffer = block_output_io_buffer->out_buffer_ptr();
                size_t count = block_output_io_buffer->out_buffer_size();
                if (count > 0) {
                    // We don't have to drain threads here because our child process is simple.
                    const char *fork_reason = p->type == INTERNAL_BLOCK_NODE ? "internal block io" : "internal function io";
                    if (!do_fork(false, fork_reason, [&] {
                            exec_write_and_exit(block_output_io_buffer->fd, buffer, count, status);
                        })) {
                        break;
                    }
                } else {
                    if (p->is_last_in_job) {
                        proc_set_last_status(j->get_flag(JOB_NEGATE) ? (!status) : status);
                    }
                    p->completed = 1;
                }

                block_output_io_buffer.reset();
                break;
            }

            case INTERNAL_BUILTIN: {
                // Handle output from builtin commands. In the general case, this means forking of a
                // worker process, that will write out the contents of the stdout and stderr buffers
                // to the correct file descriptor. Since forking is expensive, fish tries to avoid
                // it when possible.
                bool fork_was_skipped = false;

                const shared_ptr<io_data_t> stdout_io =
                    process_net_io_chain.get_io_for_fd(STDOUT_FILENO);
                const shared_ptr<io_data_t> stderr_io =
                    process_net_io_chain.get_io_for_fd(STDERR_FILENO);

                assert(builtin_io_streams.get() != NULL);
                const output_stream_t &stdout_stream = builtin_io_streams->out;
                const output_stream_t &stderr_stream = builtin_io_streams->err;

                // If we are outputting to a file, we have to actually do it, even if we have no
                // output, so that we can truncate the file. Does not apply to /dev/null.
                bool must_fork = redirection_is_to_real_file(stdout_io.get()) ||
                                 redirection_is_to_real_file(stderr_io.get());
                if (!must_fork && p->is_last_in_job) {
                    // We are handling reads directly in the main loop. Note that we may still end
                    // up forking.
                    const bool stdout_is_to_buffer = stdout_io && stdout_io->io_mode == IO_BUFFER;
                    const bool no_stdout_output = stdout_stream.empty();
                    const bool no_stderr_output = stderr_stream.empty();
                    const bool stdout_discarded = stdout_stream.output_discarded();

                    if (!stdout_discarded && no_stdout_output && no_stderr_output) {
                        // The builtin produced no output and is not inside of a pipeline. No
                        // need to fork or even output anything.
                        debug(3, L"Skipping fork: no output for internal builtin '%ls'",
                              p->argv0());
                        fork_was_skipped = true;
                    } else if (no_stderr_output && stdout_is_to_buffer) {
                        // The builtin produced no stderr, and its stdout is going to an
                        // internal buffer. There is no need to fork. This helps out the
                        // performance quite a bit in complex completion code.
                        debug(3, L"Skipping fork: buffered output for internal builtin '%ls'",
                              p->argv0());

                        io_buffer_t *io_buffer = static_cast<io_buffer_t *>(stdout_io.get());
                        io_buffer->append_from_stream(stdout_stream);
                        fork_was_skipped = true;
                    } else if (stdout_io.get() == NULL && stderr_io.get() == NULL) {
                        // We are writing to normal stdout and stderr. Just do it - no need to fork.
                        debug(3, L"Skipping fork: ordinary output for internal builtin '%ls'",
                              p->argv0());
                        const std::string outbuff = wcs2string(stdout_stream.buffer());
                        const std::string errbuff = wcs2string(stderr_stream.buffer());
                        bool builtin_io_done = do_builtin_io(outbuff.data(), outbuff.size(),
                                                             errbuff.data(), errbuff.size());
                        if (!builtin_io_done && errno != EPIPE) {
                            redirect_tty_output();  // workaround glibc bug
                            debug(0, "!builtin_io_done and errno != EPIPE");
                            show_stackframe(L'E');
                        }
                        if (stdout_discarded) p->status = STATUS_READ_TOO_MUCH;
                        fork_was_skipped = true;
                    }
                }

                if (fork_was_skipped) {
                    p->completed = 1;
                    if (p->is_last_in_job) {
                        debug(3, L"Set status of %ls to %d using short circuit", j->command_wcstr(),
                              p->status);

                        int status = p->status;
                        proc_set_last_status(j->get_flag(JOB_NEGATE) ? (!status) : status);
                    }
                } else {
                    // Ok, unfortunately, we have to do a real fork. Bummer. We work hard to make
                    // sure we don't have to wait for all our threads to exit, by arranging things
                    // so that we don't have to allocate memory or do anything except system calls
                    // in the child.
                    //
                    // These strings may contain embedded nulls, so don't treat them as C strings.
                    const std::string outbuff_str = wcs2string(stdout_stream.buffer());
                    const char *outbuff = outbuff_str.data();
                    size_t outbuff_len = outbuff_str.size();

                    const std::string errbuff_str = wcs2string(stderr_stream.buffer());
                    const char *errbuff = errbuff_str.data();
                    size_t errbuff_len = errbuff_str.size();

                    fflush(stdout);
                    fflush(stderr);
                    if (!do_fork(false, "internal builtin", [&] {
                            do_builtin_io(outbuff, outbuff_len, errbuff, errbuff_len);
                            exit_without_destructors(p->status);
                        })) {
                        break;
                    }
                }

                break;
            }

            case EXTERNAL: {
                // Get argv and envv before we fork.
                null_terminated_array_t<char> argv_array;
                convert_wide_array_to_narrow(p->get_argv_array(), &argv_array);

                // Ensure that stdin is blocking before we hand it off (see issue #176). It's a
                // little strange that we only do this with stdin and not with stdout or stderr.
                // However in practice, setting or clearing O_NONBLOCK on stdin also sets it for the
                // other two fds, presumably because they refer to the same underlying file
                // (/dev/tty?).
                make_fd_blocking(STDIN_FILENO);

                const char *const *argv = argv_array.get();
                const char *const *envv = env_export_arr();

                std::string actual_cmd_str = wcs2string(p->actual_cmd);
                const char *actual_cmd = actual_cmd_str.c_str();
                const wchar_t *file = reader_current_filename();

#if FISH_USE_POSIX_SPAWN
                // Prefer to use posix_spawn, since it's faster on some systems like OS X.
                bool use_posix_spawn = g_use_posix_spawn && can_use_posix_spawn_for_job(j, p);
                if (use_posix_spawn) {
                    g_fork_count++;  // spawn counts as a fork+exec
                    // Create posix spawn attributes and actions.
                    posix_spawnattr_t attr = posix_spawnattr_t();
                    posix_spawn_file_actions_t actions = posix_spawn_file_actions_t();
                    bool made_it = fork_actions_make_spawn_properties(&attr, &actions, j, p,
                                                                      process_net_io_chain);
                    if (made_it) {
                        // We successfully made the attributes and actions; actually call
                        // posix_spawn.
                        int spawn_ret = posix_spawn(&pid, actual_cmd, &actions, &attr,
                                                    const_cast<char *const *>(argv),
                                                    const_cast<char *const *>(envv));

                        // This usleep can be used to test for various race conditions
                        // (https://github.com/fish-shell/fish-shell/issues/360).
                        // usleep(10000);

                        if (spawn_ret != 0) {
                            safe_report_exec_error(spawn_ret, actual_cmd, argv, envv);
                            // Make sure our pid isn't set.
                            pid = 0;
                        }

                        // Clean up our actions.
                        posix_spawn_file_actions_destroy(&actions);
                        posix_spawnattr_destroy(&attr);
                    }

                    // A 0 pid means we failed to posix_spawn. Since we have no pid, we'll never get
                    // told when it's exited, so we have to mark the process as failed.
                    debug(2, L"Fork #%d, pid %d: spawn external command '%s' from '%ls'",
                          g_fork_count, pid, actual_cmd, file ? file : L"<no file>");
                    if (pid == 0) {
                        job_mark_process_as_failed(j, p);
                        exec_error = true;
                        break;
                    }

                    // these are all things do_fork() takes care of normally (for forked processes):
                    p->pid = pid;
                    child_spawned = true;
                    on_process_created(j, p->pid);

                    // We explicitly don't call set_child_group() for spawned processes because that
                    // a) isn't necessary, and b) causes issues like fish-shell/fish-shell#4715

#if defined(__GLIBC__)
                    // Unfortunately, using posix_spawn() is not the panacea it would appear to be, glibc has
                    // a penchant for using fork() instead of vfork() when posix_spawn() is called, meaning that
                    // atomicity is not guaranteed and we can get here before the child group has been set.
                    // See discussion here: https://github.com/Microsoft/WSL/issues/2997
                    // And confirmation that this persists past glibc 2.24+ here:
                    // https://github.com/fish-shell/fish-shell/issues/4715
                    if (j->get_flag(JOB_CONTROL) && getpgid(p->pid) != j->pgid) {
                        set_child_group(j, p->pid);
                    }
#else
                    // In do_fork, the pid of the child process is used as the group leader if j->pgid == 2
                    // above, posix_spawn assigned the new group a pgid equal to its own id if j->pgid == 2
                    // so this is what we do instead of calling set_child_group:
                    if (j->pgid == -2) {
                        j->pgid = pid;
                    }
#endif

                    maybe_assign_terminal(j);
                } else
#endif
                {
                    if (!do_fork(false, "external command",
                                 [&] { safe_launch_process(p, actual_cmd, argv, envv); })) {
                        break;
                    }
                }

                break;
            }

            case INTERNAL_EXEC: {
                // We should have handled exec up above.
                DIE("INTERNAL_EXEC process found in pipeline, where it should never be. Aborting.");
                break;
            }
        }

        // Close the pipe the current process uses to read from the previous process_t.
        if (pipe_current_read >= 0) {
            exec_close(pipe_current_read);
            pipe_current_read = -1;
        }

        // Close the write end too, since the curent child subprocess already has a copy of it.
        if (pipe_current_write >= 0) {
            exec_close(pipe_current_write);
            pipe_current_write = -1;
        }
    }

    // Clean up any file descriptors we left open.
    if (pipe_current_read >= 0) exec_close(pipe_current_read);
    if (pipe_current_write >= 0) exec_close(pipe_current_write);
    if (pipe_next_read >= 0) exec_close(pipe_next_read);

    // The keepalive process is no longer needed, so we terminate it with extreme prejudice.
    if (needs_keepalive) {
        kill(keepalive.pid, SIGKILL);
    }

    debug(3, L"Job is constructed");
    j->set_flag(JOB_CONSTRUCTED, true);
    if (!j->get_flag(JOB_FOREGROUND)) {
        proc_last_bg_pid = j->pgid;
        env_set(L"last_pid", ENV_GLOBAL, { to_string(proc_last_bg_pid) });
    }

    if (!exec_error) {
        job_continue(j, false);
    } else {
        // Mark the errored job as not in the foreground. I can't fully justify whether this is the
        // right place, but it prevents sanity_lose from complaining.
        j->set_flag(JOB_FOREGROUND, false);
    }
}

static int exec_subshell_internal(const wcstring &cmd, wcstring_list_t *lst, bool apply_exit_status,
                                  bool is_subcmd) {
    ASSERT_IS_MAIN_THREAD();
    bool prev_subshell = is_subshell;
    const int prev_status = proc_get_last_status();
    bool split_output = false;

    const auto ifs = env_get(L"IFS");
    if (!ifs.missing_or_empty()) {
        split_output = true;
    }

    is_subshell = true;
    int subcommand_status = -1;  // assume the worst

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may
    // be null.
    const shared_ptr<io_buffer_t> io_buffer(
        io_buffer_t::create(STDOUT_FILENO, io_chain_t(), is_subcmd ? read_byte_limit : 0));
    if (io_buffer.get() != NULL) {
        parser_t &parser = parser_t::principal_parser();
        if (parser.eval(cmd, io_chain_t(io_buffer), SUBST) == 0) {
            subcommand_status = proc_get_last_status();
        }

        io_buffer->read();
    }

    if (io_buffer->output_discarded()) subcommand_status = STATUS_READ_TOO_MUCH;

    // If the caller asked us to preserve the exit status, restore the old status. Otherwise set the
    // status of the subcommand.
    proc_set_last_status(apply_exit_status ? subcommand_status : prev_status);
    is_subshell = prev_subshell;

    if (lst == NULL || io_buffer.get() == NULL) {
        return subcommand_status;
    }

    const char *begin = io_buffer->out_buffer_ptr();
    const char *end = begin + io_buffer->out_buffer_size();
    if (split_output) {
        const char *cursor = begin;
        while (cursor < end) {
            // Look for the next separator.
            const char *stop = (const char *)memchr(cursor, '\n', end - cursor);
            const bool hit_separator = (stop != NULL);
            if (!hit_separator) {
                // If it's not found, just use the end.
                stop = end;
            }
            // Stop now points at the first character we do not want to copy.
            const wcstring wc = str2wcstring(cursor, stop - cursor);
            lst->push_back(wc);

            // If we hit a separator, skip over it; otherwise we're at the end.
            cursor = stop + (hit_separator ? 1 : 0);
        }
    } else {
        // We're not splitting output, but we still want to trim off a trailing newline.
        if (end != begin && end[-1] == '\n') {
            --end;
        }
        const wcstring wc = str2wcstring(begin, end - begin);
        lst->push_back(wc);
    }

    return subcommand_status;
}

int exec_subshell(const wcstring &cmd, std::vector<wcstring> &outputs, bool apply_exit_status,
                  bool is_subcmd) {
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, &outputs, apply_exit_status, is_subcmd);
}

int exec_subshell(const wcstring &cmd, bool apply_exit_status, bool is_subcmd) {
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, NULL, apply_exit_status, is_subcmd);
}
