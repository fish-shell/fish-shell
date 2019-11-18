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
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <type_traits>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "function.h"
#include "io.h"
#include "iothread.h"
#include "parse_tree.h"
#include "parser.h"
#include "path.h"
#include "postfork.h"
#include "proc.h"
#include "reader.h"
#include "redirection.h"
#include "signal.h"
#include "trace.h"
#include "wutil.h"  // IWYU pragma: keep

/// File descriptor redirection error message.
#define FD_ERROR _(L"An error occurred while redirecting file descriptor %d")

/// File redirection error message.
#define FILE_ERROR _(L"An error occurred while redirecting file '%ls'")

/// Base open mode to pass to calls to open.
#define OPEN_MASK 0666

/// Number of calls to fork() or posix_spawn().
static relaxed_atomic_t<int> s_fork_count{0};

void exec_close(int fd) {
    ASSERT_IS_MAIN_THREAD();

    // This may be called in a child of fork(), so don't allocate memory.
    if (fd < 0) {
        FLOG(error, L"Called close on invalid file descriptor ");
        return;
    }

    while (close(fd) == -1) {
        debug(1, FD_ERROR, fd);
        wperror(L"close");
        break;
    }
}

/// Returns true if the redirection is a file redirection to a file other than /dev/null.
static bool redirection_is_to_real_file(const shared_ptr<const io_data_t> &io) {
    bool result = false;
    if (io && io->io_mode == io_mode_t::file) {
        // It's a file redirection. Compare the path to /dev/null.
        const wcstring &path = static_cast<const io_file_t *>(io.get())->filename;
        if (path != L"/dev/null") {
            // It's not /dev/null.
            result = true;
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

    if (std::strncmp(interpreter, "#! /", 4) == 0) {
        return interpreter + 3;
    } else if (std::strncmp(interpreter, "#!/", 3) == 0) {
        return interpreter + 2;
    }

    return NULL;
}

/// This function is executed by the child process created by a call to fork(). It should be called
/// after \c child_setup_process. It calls execve to replace the fish process image with the command
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
static void launch_process_nofork(env_stack_t &vars, process_t *p) {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

    null_terminated_array_t<char> argv_array;
    convert_wide_array_to_narrow(p->get_argv_array(), &argv_array);

    auto export_vars = vars.export_arr();
    const char *const *envv = export_vars->get();
    char *actual_cmd = wcs2str(p->actual_cmd);

    // Ensure the terminal modes are what they were before we changed them.
    restore_term_mode();
    // Bounce to launch_process. This never returns.
    safe_launch_process(p, actual_cmd, argv_array.get(), envv);
}

/// Make a copy of the specified io redirection chain, but change file redirection into fd
/// redirection. This makes the redirection chain suitable for use as block-level io, since the file
/// won't be repeatedly reopened for every command in the block, which would reset the cursor
/// position.
///
/// \return true on success, false on failure. Returns the output chain and opened_fds by reference.
static bool resolve_file_redirections_to_fds(const io_chain_t &in_chain, const wcstring &pwd,
                                             io_chain_t *out_chain,
                                             std::vector<autoclose_fd_t> *out_opened_fds) {
    ASSERT_IS_MAIN_THREAD();
    assert(out_chain != NULL && out_opened_fds != NULL);
    assert(out_chain->empty());

    // Just to be clear what we do for an empty chain.
    if (in_chain.empty()) {
        return true;
    }

    bool success = true;

    // All of the FDs that we create.
    std::vector<autoclose_fd_t> opened_fds{};

    // Make our chain of redirections.
    io_chain_t result_chain;

    for (const io_data_ref_t &in : in_chain) {
        switch (in->io_mode) {
            case io_mode_t::pipe:
            case io_mode_t::bufferfill:
            case io_mode_t::fd:
            case io_mode_t::close: {
                result_chain.push_back(in);
                break;
            }
            case io_mode_t::file: {
                // We have a path-based redireciton. Resolve it to a file.
                const io_file_t *in_file = static_cast<const io_file_t *>(in.get());
                int fd = wopen(path_apply_working_directory(in_file->filename, pwd), in_file->flags,
                               OPEN_MASK);
                if (fd < 0) {
                    debug(1, FILE_ERROR, in_file->filename.c_str());
                    wperror(L"open");
                    success = false;
                    break;
                }

                opened_fds.push_back(autoclose_fd_t(fd));
                result_chain.push_back(std::make_shared<io_fd_t>(in->fd, fd, false));
                break;
            }
        }
        if (!success) {
            break;
        }
    }

    if (success) {
        *out_chain = std::move(result_chain);
        *out_opened_fds = std::move(opened_fds);
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
                          const io_chain_t &ios, std::shared_ptr<job_t> parent_job) {
    assert(parsed_source && node && "exec_helper missing source or without node");

    io_chain_t morphed_chain;
    std::vector<autoclose_fd_t> opened_fds;
    if (!resolve_file_redirections_to_fds(ios, parser.vars().get_pwd_slash(), &morphed_chain,
                                          &opened_fds)) {
        parser.set_last_statuses(statuses_t::just(STATUS_EXEC_FAIL));
        return;
    }

    parser.eval_node(parsed_source, node, morphed_chain, TOP, parent_job);

    morphed_chain.clear();
    job_reap(parser, false);
}

// Returns whether we can use posix spawn for a given process in a given job.
//
// To avoid the race between the caller calling tcsetpgrp() and the client checking the
// foreground process group, we don't use posix_spawn if we're going to foreground the process. (If
// we use fork(), we can call tcsetpgrp after the fork, before the exec, and avoid the race).
static bool can_use_posix_spawn_for_job(const std::shared_ptr<job_t> &job) {
    if (job->wants_job_control()) {  //!OCLINT(collapsible if statements)
        // We are going to use job control; therefore when we launch this job it will get its own
        // process group ID. But will it be foregrounded?
        if (job->should_claim_terminal()) {
            // It will be foregrounded, so we will call tcsetpgrp(), therefore do not use
            // posix_spawn.
            return false;
        }
    }
    return true;
}

void internal_exec(env_stack_t &vars, job_t *j, const io_chain_t &all_ios) {
    // Do a regular launch -  but without forking first...

    // child_setup_process makes sure signals are properly set up.

    // PCA This is for handling exec. Passing all_ios here matches what fish 2.0.0 and 1.x did.
    // It's known to be wrong - for example, it means that redirections bound for subsequent
    // commands in the pipeline will apply to exec. However, using exec in a pipeline doesn't
    // really make sense, so I'm not trying to fix it here.
    auto redirs = dup2_list_t::resolve_chain(all_ios);
    if (redirs && !child_setup_process(INVALID_PID, false, *redirs)) {
        // Decrement SHLVL as we're removing ourselves from the shell "stack".
        auto shlvl_var = vars.get(L"SHLVL", ENV_GLOBAL | ENV_EXPORT);
        wcstring shlvl_str = L"0";
        if (shlvl_var) {
            long shlvl = fish_wcstol(shlvl_var->as_string().c_str());
            if (!errno && shlvl > 0) {
                shlvl_str = to_string(shlvl - 1);
            }
        }
        vars.set_one(L"SHLVL", ENV_GLOBAL | ENV_EXPORT, std::move(shlvl_str));

        // launch_process _never_ returns.
        launch_process_nofork(vars, j->processes.front().get());
    } else {
        j->mut_flags().constructed = true;
        j->processes.front()->completed = 1;
        return;
    }
}

static void on_process_created(const std::shared_ptr<job_t> &j, pid_t child_pid) {
    // We only need to do this the first time a child is forked/spawned
    if (j->pgid != INVALID_PID) {
        return;
    }

    if (j->wants_job_control()) {
        j->pgid = child_pid;
    } else {
        j->pgid = getpgrp();
    }
}

/// Construct an internal process for the process p. In the background, write the data \p outdata to
/// stdout and \p errdata to stderr, respecting the io chain \p ios. For example if target_fd is 1
/// (stdout), and there is a dup2 3->1, then we need to write to fd 3. Then exit the internal
/// process.
static bool run_internal_process(process_t *p, std::string outdata, std::string errdata,
                                 io_chain_t ios) {
    p->check_generations_before_launch();

    // We want both the dup2s and the io_chain_ts to be kept alive by the background thread, because
    // they may own an fd that we want to write to. Move them all to a shared_ptr. The strings as
    // well (they may be long).
    // Construct a little helper struct to make it simpler to move into our closure without copying.
    struct write_fields_t {
        int src_outfd{-1};
        std::string outdata{};

        int src_errfd{-1};
        std::string errdata{};

        io_chain_t ios{};
        maybe_t<dup2_list_t> dup2s{};
        std::shared_ptr<internal_proc_t> internal_proc{};

        proc_status_t success_status{};

        bool skip_out() const { return outdata.empty() || src_outfd < 0; }

        bool skip_err() const { return errdata.empty() || src_errfd < 0; }
    };

    auto f = std::make_shared<write_fields_t>();
    f->outdata = std::move(outdata);
    f->errdata = std::move(errdata);

    // Construct and assign the internal process to the real process.
    p->internal_proc_ = std::make_shared<internal_proc_t>();
    f->internal_proc = p->internal_proc_;

    FLOGF(proc_internal_proc, L"Created internal proc %llu to write output for proc '%ls'",
          p->internal_proc_->get_id(), p->argv0());

    // Resolve the IO chain.
    // Note it's important we do this even if we have no out or err data, because we may have been
    // asked to truncate a file (e.g. `echo -n '' > /tmp/truncateme.txt'). The open() in the dup2
    // list resolution will ensure this happens.
    f->dup2s = dup2_list_t::resolve_chain(ios);
    if (!f->dup2s) {
        return false;
    }

    // Figure out which source fds to write to. If they are closed (unlikely) we just exit
    // successfully.
    f->src_outfd = f->dup2s->fd_for_target_fd(STDOUT_FILENO);
    f->src_errfd = f->dup2s->fd_for_target_fd(STDERR_FILENO);

    // If we have nothing to write we can elide the thread.
    // TODO: support eliding output to /dev/null.
    if (f->skip_out() && f->skip_err()) {
        f->internal_proc->mark_exited(proc_status_t::from_exit_code(EXIT_SUCCESS));
        return true;
    }

    // Ensure that ios stays alive, it may own fds.
    f->ios = ios;

    // If our process is a builtin, it will have already set its status value. Make sure we
    // propagate that if our I/O succeeds and don't read it on a background thread. TODO: have
    // builtin_run provide this directly, rather than setting it in the process.
    f->success_status = p->status;

    iothread_perform([f]() {
        proc_status_t status = f->success_status;
        if (!f->skip_out()) {
            ssize_t ret = write_loop(f->src_outfd, f->outdata.data(), f->outdata.size());
            if (ret < 0) {
                if (errno != EPIPE) {
                    wperror(L"write");
                }
                if (status.is_success()) {
                    status = proc_status_t::from_exit_code(1);
                }
            }
        }
        if (!f->skip_err()) {
            ssize_t ret = write_loop(f->src_errfd, f->errdata.data(), f->errdata.size());
            if (ret < 0) {
                if (errno != EPIPE) {
                    wperror(L"write");
                }
                if (status.is_success()) {
                    status = proc_status_t::from_exit_code(1);
                }
            }
        }
        f->internal_proc->mark_exited(status);
    });
    return true;
}

/// Call fork() as part of executing a process \p p in a job \j. Execute \p child_action in the
/// context of the child. Returns true if fork succeeded, false if fork failed.
static bool fork_child_for_process(const std::shared_ptr<job_t> &job, process_t *p,
                                   const dup2_list_t &dup2s, bool drain_threads,
                                   const char *fork_type,
                                   const std::function<void()> &child_action) {
    pid_t pid = execute_fork(drain_threads);
    if (pid == 0) {
        // This is the child process. Setup redirections, print correct output to
        // stdout and stderr, and then exit.
        maybe_t<pid_t> new_termowner{};
        p->pid = getpid();
        child_set_group(job.get(), p);
        child_setup_process(job->should_claim_terminal() ? job->pgid : INVALID_PID, true, dup2s);
        child_action();
        DIE("Child process returned control to fork_child lambda!");
    }

    if (pid < 0) {
        debug(1, L"Failed to fork %s!\n", fork_type);
        job_mark_process_as_failed(job, p);
        return false;
    }

    // This is the parent process. Store away information on the child, and
    // possibly give it control over the terminal.
    s_fork_count++;
    FLOGF(exec_fork, L"Fork #%d, pid %d: %s for '%ls'", int(s_fork_count), pid, fork_type,
          p->argv0());

    p->pid = pid;
    on_process_created(job, p->pid);
    set_child_group(job.get(), p->pid);
    terminal_maybe_give_to_job(job.get(), false);
    return true;
}

/// Execute an internal builtin. Given a parser, a job within that parser, and a process within that
/// job corresponding to a builtin, execute the builtin with the given streams. If pipe_read is set,
/// assign stdin to it; otherwise infer stdin from the IO chain.
/// \return true on success, false if there is an exec error.
static bool exec_internal_builtin_proc(parser_t &parser, const std::shared_ptr<job_t> &j,
                                       process_t *p, const io_pipe_t *pipe_read,
                                       const io_chain_t &proc_io_chain, io_streams_t &streams) {
    assert(p->type == process_type_t::builtin && "Process must be a builtin");
    int local_builtin_stdin = STDIN_FILENO;
    autoclose_fd_t locally_opened_stdin{};

    // If this is the first process, check the io redirections and see where we should
    // be reading from.
    if (pipe_read) {
        local_builtin_stdin = pipe_read->pipe_fd();
    } else if (const auto in = proc_io_chain.io_for_fd(STDIN_FILENO)) {
        switch (in->io_mode) {
            case io_mode_t::fd: {
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
            case io_mode_t::pipe: {
                const io_pipe_t *in_pipe = static_cast<const io_pipe_t *>(in.get());
                if (in_pipe->fd == STDIN_FILENO) {
                    local_builtin_stdin = in_pipe->pipe_fd();
                }
                break;
            }
            case io_mode_t::file: {
                const io_file_t *in_file = static_cast<const io_file_t *>(in.get());
                locally_opened_stdin =
                    autoclose_fd_t{wopen(in_file->filename, in_file->flags, OPEN_MASK)};
                if (!locally_opened_stdin.valid()) {
                    debug(1, FILE_ERROR, in_file->filename.c_str());
                    wperror(L"open");
                }
                local_builtin_stdin = locally_opened_stdin.fd();
                break;
            }
            case io_mode_t::close: {
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
        // that's not io_mode_t::close.
        const shared_ptr<const io_data_t> stdin_io = p->io_chain().io_for_fd(STDIN_FILENO);
        stdin_is_directly_redirected = stdin_io && stdin_io->io_mode != io_mode_t::close;
    }

    streams.stdin_fd = local_builtin_stdin;
    streams.out_is_redirected = proc_io_chain.io_for_fd(STDOUT_FILENO) != nullptr;
    streams.err_is_redirected = proc_io_chain.io_for_fd(STDERR_FILENO) != nullptr;
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
    const bool fg = j->is_foreground();
    j->mut_flags().foreground = false;

    // Note this call may block for a long time, while the builtin performs I/O.
    p->status = builtin_run(parser, j->pgid, p->get_argv(), streams);

    // Restore the fg flag, which is temporarily set to false during builtin
    // execution so as not to confuse some job-handling builtins.
    j->mut_flags().foreground = fg;

    return true;  // "success"
}

/// Handle output from a builtin, by printing the contents of builtin_io_streams to the redirections
/// given in io_chain.
static bool handle_builtin_output(parser_t &parser, const std::shared_ptr<job_t> &j, process_t *p,
                                  io_chain_t *io_chain, const io_streams_t &builtin_io_streams) {
    assert(p->type == process_type_t::builtin && "Process is not a builtin");

    const output_stream_t &stdout_stream = builtin_io_streams.out;
    const output_stream_t &stderr_stream = builtin_io_streams.err;

    // Mark if we discarded output.
    if (stdout_stream.buffer().discarded()) {
        p->status = proc_status_t::from_exit_code(STATUS_READ_TOO_MUCH);
    }

    // We will try to elide constructing an internal process. However if the output is going to a
    // real file, we have to do it. For example in `echo -n > file.txt` we proceed to open file.txt
    // even though there is no output, so that it is properly truncated.
    const shared_ptr<const io_data_t> stdout_io = io_chain->io_for_fd(STDOUT_FILENO);
    const shared_ptr<const io_data_t> stderr_io = io_chain->io_for_fd(STDERR_FILENO);
    bool must_use_process =
        redirection_is_to_real_file(stdout_io) || redirection_is_to_real_file(stderr_io);

    // If we are directing output to a buffer, then we can just transfer it directly without needing
    // to write to the bufferfill pipe. Note this is how we handle explicitly separated stdout
    // output (i.e. string split0) which can't really be sent through a pipe.
    // TODO: we're sloppy about handling explicitly separated output.
    // Theoretically we could have explicitly separated output on stdout and also stderr output; in
    // that case we ought to thread the exp-sep output through to the io buffer. We're getting away
    // with this because the only thing that can output exp-sep output is `string split0` which
    // doesn't also produce stderr. Also note that we never send stderr to a buffer, so there's no
    // need for a similar check for stderr.
    bool stdout_done = false;
    if (stdout_io && stdout_io->io_mode == io_mode_t::bufferfill) {
        auto stdout_buffer = static_cast<const io_bufferfill_t *>(stdout_io.get())->buffer();
        stdout_buffer->append_from_stream(stdout_stream);
        stdout_done = true;
    }

    // Figure out any data remaining to write. We may have none in which case we can short-circuit.
    std::string outbuff = stdout_done ? std::string{} : wcs2string(stdout_stream.contents());
    std::string errbuff = wcs2string(stderr_stream.contents());

    // If we have no redirections for stdout/stderr, just write them directly.
    if (!stdout_io && !stderr_io) {
        bool did_err = false;
        if (write_loop(STDOUT_FILENO, outbuff.data(), outbuff.size()) < 0) {
            if (errno != EPIPE) {
                did_err = true;
                FLOG(error, L"Error while writing to stdout");
                wperror(L"write_loop");
            }
        }
        if (write_loop(STDERR_FILENO, errbuff.data(), errbuff.size()) < 0) {
            if (errno != EPIPE && !did_err) {
                did_err = true;
                FLOG(error, L"Error while writing to stderr");
                wperror(L"write_loop");
            }
        }
        if (did_err) {
            redirect_tty_output();  // workaround glibc bug
            FLOG(error, L"!builtin_io_done and errno != EPIPE");
            show_stackframe(L'E');
        }
        // Clear the buffers to indicate we finished.
        outbuff.clear();
        errbuff.clear();
    }

    if (!must_use_process && outbuff.empty() && errbuff.empty()) {
        // We do not need to construct a background process.
        // TODO: factor this job-status-setting stuff into a single place.
        p->completed = 1;
        if (p->is_last_in_job) {
            FLOGF(exec_job_status, L"Set status of job %d (%ls) to %d using short circuit",
                  j->job_id, j->preview().c_str(), p->status);
            parser.set_last_statuses(j->get_statuses());
        }
        return true;
    } else {
        // Construct and run our background process.
        fflush(stdout);
        fflush(stderr);
        return run_internal_process(p, std::move(outbuff), std::move(errbuff), *io_chain);
    }
}

/// Executes an external command.
/// \return true on success, false if there is an exec error.
static bool exec_external_command(parser_t &parser, const std::shared_ptr<job_t> &j, process_t *p,
                                  const io_chain_t &proc_io_chain) {
    assert(p->type == process_type_t::external && "Process is not external");
    // Get argv and envv before we fork.
    null_terminated_array_t<char> argv_array;
    convert_wide_array_to_narrow(p->get_argv_array(), &argv_array);

    // Convert our IO chain to a dup2 sequence.
    auto dup2s = dup2_list_t::resolve_chain(proc_io_chain);
    if (!dup2s) return false;

    // Ensure that stdin is blocking before we hand it off (see issue #176). It's a
    // little strange that we only do this with stdin and not with stdout or stderr.
    // However in practice, setting or clearing O_NONBLOCK on stdin also sets it for the
    // other two fds, presumably because they refer to the same underlying file
    // (/dev/tty?).
    make_fd_blocking(STDIN_FILENO);

    auto export_arr = parser.vars().export_arr();
    const char *const *argv = argv_array.get();
    const char *const *envv = export_arr->get();

    std::string actual_cmd_str = wcs2string(p->actual_cmd);
    const char *actual_cmd = actual_cmd_str.c_str();
    const wchar_t *file = parser.libdata().current_filename;

#if FISH_USE_POSIX_SPAWN
    // Prefer to use posix_spawn, since it's faster on some systems like OS X.
    bool use_posix_spawn = g_use_posix_spawn && can_use_posix_spawn_for_job(j);
    if (use_posix_spawn) {
        s_fork_count++;  // spawn counts as a fork+exec
        // Create posix spawn attributes and actions.
        pid_t pid = 0;
        posix_spawnattr_t attr = posix_spawnattr_t();
        posix_spawn_file_actions_t actions = posix_spawn_file_actions_t();
        bool made_it = fork_actions_make_spawn_properties(&attr, &actions, j.get(), *dup2s);
        if (made_it) {
            // We successfully made the attributes and actions; actually call
            // posix_spawn.
            int spawn_ret =
                posix_spawn(&pid, actual_cmd, &actions, &attr, const_cast<char *const *>(argv),
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
        FLOGF(exec_fork, L"Fork #%d, pid %d: spawn external command '%s' from '%ls'",
              int(s_fork_count), pid, actual_cmd, file ? file : L"<no file>");
        if (pid == 0) {
            job_mark_process_as_failed(j, p);
            return false;
        }

        // these are all things do_fork() takes care of normally (for forked processes):
        p->pid = pid;
        on_process_created(j, p->pid);

        // We explicitly don't call set_child_group() for spawned processes because that
        // a) isn't necessary, and b) causes issues like fish-shell/fish-shell#4715

#if defined(__GLIBC__)
        // Unfortunately, using posix_spawn() is not the panacea it would appear to be,
        // glibc has a penchant for using fork() instead of vfork() when posix_spawn() is
        // called, meaning that atomicity is not guaranteed and we can get here before the
        // child group has been set. See discussion here:
        // https://github.com/Microsoft/WSL/issues/2997 And confirmation that this persists
        // past glibc 2.24+ here: https://github.com/fish-shell/fish-shell/issues/4715
        if (j->wants_job_control() && getpgid(p->pid) != j->pgid) {
            set_child_group(j.get(), p->pid);
        }
#else
        // In do_fork, the pid of the child process is used as the group leader if j->pgid
        // invalid, posix_spawn assigned the new group a pgid equal to its own id if
        // j->pgid was invalid, so this is what we do instead of calling set_child_group
        if (j->pgid == INVALID_PID) {
            j->pgid = pid;
        }
#endif

        terminal_maybe_give_to_job(j.get(), false);
    } else
#endif
    {
        if (!fork_child_for_process(j, p, *dup2s, false, "external command",
                                    [&] { safe_launch_process(p, actual_cmd, argv, envv); })) {
            return false;
        }
    }

    return true;
}

// Given that we are about to execute a function, push a function block and set up the
// variable environment.
static block_t *function_prepare_environment(parser_t &parser, wcstring_list_t argv,
                                             const function_properties_t &props) {
    // Extract the function name and remaining arguments.
    wcstring func_name;
    if (!argv.empty()) {
        // Extract and remove the function name from argv.
        func_name = std::move(*argv.begin());
        argv.erase(argv.begin());
    }
    block_t *fb = parser.push_block(block_t::function_block(func_name, argv, props.shadow_scope));
    auto &vars = parser.vars();

    // Setup the environment for the function. There are three components of the environment:
    // 1. named arguments
    // 2. inherited variables
    // 3. argv

    size_t idx = 0;
    for (const wcstring &named_arg : props.named_arguments) {
        if (idx < argv.size()) {
            vars.set_one(named_arg, ENV_LOCAL | ENV_USER, argv.at(idx));
        } else {
            vars.set_empty(named_arg, ENV_LOCAL | ENV_USER);
        }
        idx++;
    }

    for (const auto &kv : props.inherit_vars) {
        vars.set(kv.first, ENV_LOCAL | ENV_USER, kv.second);
    }

    vars.set_argv(std::move(argv));
    return fb;
}

// Given that we are done executing a function, restore the environment.
static void function_restore_environment(parser_t &parser, const block_t *block) {
    parser.pop_block(block);

    // If we returned due to a return statement, then stop returning now.
    parser.libdata().returning = false;
}

/// Execute a block node or function "process".
/// \p user_ios contains the list of user-specified ios, used so we can avoid stomping on them with
/// our pipes.
/// \p allow_buffering if true, permit buffering the output.
/// \return true on success, false on error.
static bool exec_block_or_func_process(parser_t &parser, std::shared_ptr<job_t> j, process_t *p,
                                       const io_chain_t &user_ios, io_chain_t io_chain,
                                       bool allow_buffering) {
    assert((p->type == process_type_t::function || p->type == process_type_t::block_node) &&
           "Unexpected process type");

    // Create an output buffer if we're piping to another process.
    shared_ptr<io_bufferfill_t> block_output_bufferfill{};
    if (!p->is_last_in_job && allow_buffering) {
        // Be careful to handle failure, e.g. too many open fds.
        block_output_bufferfill = io_bufferfill_t::create(user_ios);
        if (!block_output_bufferfill) {
            job_mark_process_as_failed(j, p);
            return false;
        }
        // Teach the job about its bufferfill, and add it to our io chain.
        io_chain.push_back(block_output_bufferfill);
    }

    if (p->type == process_type_t::function) {
        auto props = function_get_properties(p->argv0());
        if (!props) {
            FLOGF(error, _(L"Unknown function '%ls'"), p->argv0());
            return false;
        }

        const block_t *fb =
            function_prepare_environment(parser, p->get_argv_array().to_list(), *props);
        internal_exec_helper(parser, props->parsed_source, props->body_node, io_chain, j);
        function_restore_environment(parser, fb);
    } else {
        assert(p->type == process_type_t::block_node);
        assert(p->block_node_source && p->internal_block_node && "Process is missing node info");
        internal_exec_helper(parser, p->block_node_source, p->internal_block_node, io_chain, j);
    }

    int status = parser.get_last_status();
    // FIXME: setting the status this way is dangerous nonsense, we need to decode the status
    // properly if it was a signal.
    p->status = proc_status_t::from_exit_code(status);

    // If we have a block output buffer, populate it now.
    if (!block_output_bufferfill) {
        // No buffer, so we exit directly. This means we have to manually set the exit
        // status.
        p->completed = 1;
        if (p->is_last_in_job) {
            parser.set_last_statuses(j->get_statuses());
        }
        return true;
    }
    assert(block_output_bufferfill && "Must have a block output bufferfiller");

    // Remove our write pipe and forget it. This may close the pipe, unless another thread has
    // claimed it (background write) or another process has inherited it.
    io_chain.remove(block_output_bufferfill);
    auto block_output_buffer = io_bufferfill_t::finish(std::move(block_output_bufferfill));

    std::string buffer_contents = block_output_buffer->buffer().newline_serialized();
    if (!buffer_contents.empty()) {
        return run_internal_process(p, std::move(buffer_contents), {} /*errdata*/, io_chain);
    } else {
        if (p->is_last_in_job) {
            parser.set_last_statuses(j->get_statuses());
        }
        p->completed = 1;
    }
    return true;
}

/// Executes a process \p in job \j, using the pipes \p pipes (which may have invalid fds if this is
/// the first or last process).
/// \p deferred_pipes represents the pipes from our deferred process; if set ensure they get closed
/// in any child. If \p is_deferred_run is true, then this is a deferred run; this affects how
/// certain buffering works. \returns true on success, false on exec error.
static bool exec_process_in_job(parser_t &parser, process_t *p, std::shared_ptr<job_t> j,
                                autoclose_pipes_t pipes, const io_chain_t &all_ios,
                                const autoclose_pipes_t &deferred_pipes, size_t stdout_read_limit,
                                bool is_deferred_run = false) {
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

    // Maybe trace this process.
    // TODO: 'and' and 'or' will not show.
    if (trace_enabled(parser)) {
        trace_argv(parser, nullptr, p->get_argv_array().to_list());
    }

    // The IO chain for this process.
    io_chain_t process_net_io_chain = j->block_io_chain();
    if (pipes.write.valid()) {
        process_net_io_chain.push_back(std::make_shared<io_pipe_t>(
            p->pipe_write_fd, false /* not input */, std::move(pipes.write)));
    }

    // The explicit IO redirections associated with the process.
    process_net_io_chain.append(p->io_chain());

    // Read pipe goes last.
    shared_ptr<io_pipe_t> pipe_read{};
    if (pipes.read.valid()) {
        pipe_read =
            std::make_shared<io_pipe_t>(STDIN_FILENO, true /* input */, std::move(pipes.read));
        process_net_io_chain.push_back(pipe_read);
    }

    // If we have stashed pipes, make sure those get closed in the child.
    for (const autoclose_fd_t *afd : {&deferred_pipes.read, &deferred_pipes.write}) {
        if (afd->valid()) {
            process_net_io_chain.push_back(std::make_shared<io_close_t>(afd->fd()));
        }
    }

    if (p->type != process_type_t::block_node) {
        // An simple `begin ... end` should not be considered an execution of a command.
        parser.libdata().exec_count++;
    }

    // Execute the process.
    p->check_generations_before_launch();
    switch (p->type) {
        case process_type_t::function:
        case process_type_t::block_node: {
            // Allow buffering unless this is a deferred run. If deferred, then processes after us
            // were already launched, so they are ready to receive (or reject) our output.
            bool allow_buffering = !is_deferred_run;
            auto cached_exec_count = parser.libdata().exec_count;
            if (!exec_block_or_func_process(parser, j, p, all_ios, process_net_io_chain,
                                            allow_buffering)) {
                return false;
            }

            // Functions are basically treated as named blocks, and this is the only place we can
            // distinguish between them. A block by default does not touch $status, on the other
            // hand, calling an empty function should clear $status.
            if (parser.libdata().exec_count == cached_exec_count &&
                p->type == process_type_t::function) {
                p->status = proc_status_t::from_exit_code(0);
            }

            break;
        }

        case process_type_t::builtin: {
            io_streams_t builtin_io_streams{stdout_read_limit};
            if (!exec_internal_builtin_proc(parser, j, p, pipe_read.get(), process_net_io_chain,
                                            builtin_io_streams)) {
                return false;
            }
            if (!handle_builtin_output(parser, j, p, &process_net_io_chain, builtin_io_streams)) {
                return false;
            }
            break;
        }

        case process_type_t::external: {
            if (!exec_external_command(parser, j, p, process_net_io_chain)) {
                return false;
            }
            break;
        }

        case process_type_t::exec: {
            // We should have handled exec up above.
            DIE("process_type_t::exec process found in pipeline, where it should never be. "
                "Aborting.");
            break;
        }
    }
    return true;
}

// Do we have a fish internal process that pipes into a real process? If so, we are going to
// launch it last (if there's more than one, just the last one). That is to prevent buffering
// from blocking further processes. See #1396.
// Example:
//   for i in (seq 1 5); sleep 1; echo $i; end | cat
// This should show the output as it comes, not buffer until the end.
// Any such process (only one per job) will be called the "deferred" process.
static process_t *get_deferred_process(const shared_ptr<job_t> &j) {
    // By enumerating in reverse order, we can avoid walking the entire list
    for (auto i = j->processes.rbegin(); i != j->processes.rend(); ++i) {
        const auto &p = *i;
        if (p->type == process_type_t::exec) {
            // No tail reordering for execs.
            return nullptr;
        } else if (p->type != process_type_t::external) {
            if (p->is_last_in_job) {
                return nullptr;
            }
            return p.get();
        }
    }
    return nullptr;
}

/// \return true if fish should claim the process group for this job.
/// This is true if there is at least one external process and if the first process is fish code.
static bool should_claim_process_group_for_job(const shared_ptr<job_t> &j) {
    // Check if there's an external process.
    // See #6011.
    bool has_external = false;
    for (const auto &p : j->processes) {
        if (p->type == process_type_t::external) {
            has_external = true;
            break;
        }
    }
    if (!has_external) {
        return false;
    }

    // Check the first process.
    // The terminal owner has to be the process which is permitted to read from stdin.
    // This is the first process in the pipeline. When executing, a process in the job will
    // claim the pgrp if it's not set; therefore set it according to the first process.
    switch (j->processes.front()->type) {
        case process_type_t::builtin:
        case process_type_t::function:
        case process_type_t::block_node:
            // These are run internal to fish.
            return true;
        case process_type_t::external:
        case process_type_t::exec:
            // External will get its own pgroup after fork.
            // exec will retain the pgroup.
            return false;
    }
    DIE("unreachable");
}

bool exec_job(parser_t &parser, shared_ptr<job_t> j) {
    assert(j && "null job_t passed to exec_job!");

    // Set to true if something goes wrong while executing the job, in which case the cleanup
    // code will kick in.
    bool exec_error = false;

    // If fish was invoked with -n or --no-execute, then no_exec will be set and we do nothing.
    if (no_exec()) {
        return true;
    }

    // Check to see if we should reclaim the foreground pgrp after the job finishes or stops.
    const bool reclaim_foreground_pgrp = (tcgetpgrp(STDIN_FILENO) == getpgrp());

    const std::shared_ptr<job_t> parent_job = j->get_parent();

    // Perhaps inherit our parent's pgid and job control flag.
    if (parent_job && parent_job->pgid != INVALID_PID) {
        j->pgid = parent_job->pgid;
        j->mut_flags().job_control = true;
    }

    if (j->pgid == INVALID_PID && should_claim_process_group_for_job(j)) {
        j->pgid = getpgrp();
    }

    size_t stdout_read_limit = 0;
    io_chain_t all_ios = j->all_io_redirections();

    // The read limit is dictated by the last bufferfill.
    for (auto &io : all_ios) {
        if ((io->io_mode == io_mode_t::bufferfill)) {
            const auto *bf = static_cast<const io_bufferfill_t *>(io.get());
            stdout_read_limit = bf->buffer()->read_limit();
        }
    }

    // Handle an exec call.
    if (j->processes.front()->type == process_type_t::exec) {
        internal_exec(parser.vars(), j.get(), all_ios);
        // internal_exec only returns if it failed to set up redirections.
        // In case of an successful exec, this code is not reached.
        bool status = j->flags().negate ? 0 : 1;
        parser.set_last_statuses(statuses_t::just(status));
        return false;
    }

    // Get the deferred process, if any. We will have to remember its pipes.
    autoclose_pipes_t deferred_pipes;
    process_t *const deferred_process = get_deferred_process(j);

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
    autoclose_fd_t pipe_next_read;
    for (const auto &p : j->processes) {
        // proc_pipes is the pipes applied to this process. That is, it is the read end
        // containing the output of the previous process (if any), plus the write end that will
        // output to the next process (if any).
        autoclose_pipes_t proc_pipes;
        proc_pipes.read = std::move(pipe_next_read);
        if (!p->is_last_in_job) {
            auto pipes = make_autoclose_pipes(all_ios);
            if (!pipes) {
                debug(1, PIPE_ERROR);
                wperror(L"pipe");
                job_mark_process_as_failed(j, p.get());
                exec_error = true;
                break;
            }
            pipe_next_read = std::move(pipes->read);
            proc_pipes.write = std::move(pipes->write);
        }

        if (p.get() == deferred_process) {
            deferred_pipes = std::move(proc_pipes);
        } else {
            if (!exec_process_in_job(parser, p.get(), j, std::move(proc_pipes), all_ios,
                                     deferred_pipes, stdout_read_limit)) {
                exec_error = true;
                break;
            }
        }
    }
    pipe_next_read.close();

    // Now execute any deferred process.
    if (!exec_error && deferred_process) {
        assert(deferred_pipes.write.valid() && "Deferred process should always have a write pipe");
        if (!exec_process_in_job(parser, deferred_process, j, std::move(deferred_pipes), all_ios,
                                 {}, stdout_read_limit, true)) {
            exec_error = true;
        }
    }

    FLOGF(exec_job_exec, L"Executed job %d from command '%ls' with pgrp %d", j->job_id,
          j->command_wcstr(), j->pgid);

    j->mut_flags().constructed = true;
    if (!j->is_foreground()) {
        parser.vars().set_one(L"last_pid", ENV_GLOBAL, to_string(j->pgid));
    }

    if (exec_error) {
        return false;
    }

    j->continue_job(parser, reclaim_foreground_pgrp, false);
    return true;
}

static int exec_subshell_internal(const wcstring &cmd, parser_t &parser, wcstring_list_t *lst,
                                  bool apply_exit_status, bool is_subcmd) {
    ASSERT_IS_MAIN_THREAD();
    bool prev_subshell = parser.libdata().is_subshell;
    auto prev_statuses = parser.get_last_statuses();
    bool split_output = false;

    const auto ifs = parser.vars().get(L"IFS");
    if (!ifs.missing_or_empty()) {
        split_output = true;
    }

    parser.libdata().is_subshell = true;
    auto subcommand_statuses = statuses_t::just(-1);  // assume the worst

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may
    // be null.
    size_t read_limit = is_subcmd ? read_byte_limit : 0;
    std::shared_ptr<io_buffer_t> buffer;
    if (auto bufferfill = io_bufferfill_t::create(io_chain_t{}, read_limit)) {
        if (parser.eval(cmd, io_chain_t{bufferfill}, SUBST) == 0) {
            subcommand_statuses = parser.get_last_statuses();
        }
        buffer = io_bufferfill_t::finish(std::move(bufferfill));
    }

    if (buffer && buffer->buffer().discarded()) {
        subcommand_statuses = statuses_t::just(STATUS_READ_TOO_MUCH);
    }

    // If the caller asked us to preserve the exit status, restore the old status. Otherwise set the
    // status of the subcommand.
    if (apply_exit_status) {
        parser.set_last_statuses(subcommand_statuses);
    } else {
        parser.set_last_statuses(std::move(prev_statuses));
    }

    parser.libdata().is_subshell = prev_subshell;

    if (lst == NULL || !buffer) {
        return subcommand_statuses.status;
    }
    // Walk over all the elements.
    for (const auto &elem : buffer->buffer().elements()) {
        if (elem.is_explicitly_separated()) {
            // Just append this one.
            lst->push_back(str2wcstring(elem.contents));
            continue;
        }

        // Not explicitly separated. We have to split it explicitly.
        assert(!elem.is_explicitly_separated() && "should not be explicitly separated");
        const char *begin = elem.contents.data();
        const char *end = begin + elem.contents.size();
        if (split_output) {
            const char *cursor = begin;
            while (cursor < end) {
                // Look for the next separator.
                const char *stop = (const char *)std::memchr(cursor, '\n', end - cursor);
                const bool hit_separator = (stop != NULL);
                if (!hit_separator) {
                    // If it's not found, just use the end.
                    stop = end;
                }
                // Stop now points at the first character we do not want to copy.
                lst->push_back(str2wcstring(cursor, stop - cursor));

                // If we hit a separator, skip over it; otherwise we're at the end.
                cursor = stop + (hit_separator ? 1 : 0);
            }
        } else {
            // We're not splitting output, but we still want to trim off a trailing newline.
            if (end != begin && end[-1] == '\n') {
                --end;
            }
            lst->push_back(str2wcstring(begin, end - begin));
        }
    }

    return subcommand_statuses.status;
}

int exec_subshell(const wcstring &cmd, parser_t &parser, wcstring_list_t &outputs,
                  bool apply_exit_status, bool is_subcmd) {
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, parser, &outputs, apply_exit_status, is_subcmd);
}

int exec_subshell(const wcstring &cmd, parser_t &parser, bool apply_exit_status, bool is_subcmd) {
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, parser, NULL, apply_exit_status, is_subcmd);
}
