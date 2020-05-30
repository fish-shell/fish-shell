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
#include "null_terminated_array.h"
#include "parse_tree.h"
#include "parser.h"
#include "path.h"
#include "postfork.h"
#include "proc.h"
#include "reader.h"
#include "redirection.h"
#include "signal.h"
#include "timer.h"
#include "trace.h"
#include "wutil.h"  // IWYU pragma: keep

/// Number of calls to fork() or posix_spawn().
static relaxed_atomic_t<int> s_fork_count{0};

pgroup_provenance_t get_pgroup_provenance(const shared_ptr<job_t> &j,
                                          const job_lineage_t &lineage) {
    bool first_proc_is_internal = j->processes.front()->is_internal();
    bool has_internal = j->has_internal_proc();
    bool has_external = j->has_external_proc();
    assert(first_proc_is_internal ? has_internal : has_external);

    if (lineage.parent_pgid.has_value() && j->is_foreground()) {
        // Our lineage indicates a pgid. This means the job is "nested" as a function or block
        // inside another job, which has a real pgroup. We're going to use that, unless it's
        // backgrounded, in which case it should not inherit a pgroup.
        return pgroup_provenance_t::lineage;
    } else if (!j->wants_job_control()) {
        // This job doesn't need job control, it can just live in the fish pgroup.
        return pgroup_provenance_t::fish_itself;
    } else if (has_external && !first_proc_is_internal) {
        // The first process is external, it will own the pgroup.
        return pgroup_provenance_t::first_external_proc;
    } else if (has_external && first_proc_is_internal) {
        // The terminal owner has to be the process which is permitted to read from stdin.
        // This is the first process in the pipeline. When executing, a process in the job will
        // claim the pgrp if it's not set; therefore set it according to the first process.
        // Only do this if there's an external process - see #6011.
        return pgroup_provenance_t::fish_itself;
    } else {
        assert(has_internal && !has_external && "Should be internal only");
        // This job consists of only internal functions or builtins; we do not need to assign a
        // pgroup (yet).
        return pgroup_provenance_t::unassigned;
    }
}

/// This function is executed by the child process created by a call to fork(). It should be called
/// after \c child_setup_process. It calls execve to replace the fish process image with the command
/// specified in \c p. It never returns. Called in a forked child! Do not allocate memory, etc.
[[noreturn]] static void safe_launch_process(process_t *p, const char *actual_cmd,
                                             const char *const *cargv, const char *const *cenvv) {
    UNUSED(p);
    int err;

    // This function never returns, so we take certain liberties with constness.
    const auto envv = const_cast<char *const *>(cenvv);
    const auto argv = const_cast<char *const *>(cargv);

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
                if (argv2[i] == nullptr) break;
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
[[noreturn]] static void launch_process_nofork(env_stack_t &vars, process_t *p) {
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

// Returns whether we can use posix spawn for a given process in a given job.
//
// To avoid the race between the caller calling tcsetpgrp() and the client checking the
// foreground process group, we don't use posix_spawn if we're going to foreground the process. (If
// we use fork(), we can call tcsetpgrp after the fork, before the exec, and avoid the race).
static bool can_use_posix_spawn_for_job(const std::shared_ptr<job_t> &job,
                                        const dup2_list_t &dup2s) {
    // Hack - do not use posix_spawn if there are self-fd redirections.
    // For example if you were to write:
    //   cmd 6< /dev/null
    // it is possible that the open() of /dev/null would result in fd 6. Here even if we attempted
    // to add a dup2 action, it would be ignored and the CLO_EXEC bit would remain. So don't use
    // posix_spawn in this case; instead we'll call fork() and clear the CLO_EXEC bit manually.
    for (const auto &action : dup2s.get_actions()) {
        if (action.src == action.target) return false;
    }
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

static void internal_exec(env_stack_t &vars, job_t *j, const io_chain_t &block_io) {
    // Do a regular launch -  but without forking first...
    process_t *p = j->processes.front().get();
    io_chain_t all_ios = block_io;
    if (!all_ios.append_from_specs(p->redirection_specs(), vars.get_pwd_slash())) {
        return;
    }

    // child_setup_process makes sure signals are properly set up.
    dup2_list_t redirs = dup2_list_t::resolve_chain(all_ios);
    if (child_setup_process(INVALID_PID, *j, false, redirs) == 0) {
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
        launch_process_nofork(vars, p);
    }
}

/// If our pgroup assignment mode wants us to use the first external proc, then apply it here.
static void maybe_assign_pgid_from_child(const std::shared_ptr<job_t> &j, pid_t child_pid) {
    // If our assignment mode is the first process, then assign it.
    if (j->pgid == INVALID_PID &&
        j->pgroup_provenance == pgroup_provenance_t::first_external_proc) {
        j->pgid = child_pid;
    }
}

/// Construct an internal process for the process p. In the background, write the data \p outdata to
/// stdout and \p errdata to stderr, respecting the io chain \p ios. For example if target_fd is 1
/// (stdout), and there is a dup2 3->1, then we need to write to fd 3. Then exit the internal
/// process.
static void run_internal_process(process_t *p, std::string &&outdata, std::string &&errdata,
                                 const io_chain_t &ios) {
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

    // Figure out which source fds to write to. If they are closed (unlikely) we just exit
    // successfully.
    f->src_outfd = f->dup2s->fd_for_target_fd(STDOUT_FILENO);
    f->src_errfd = f->dup2s->fd_for_target_fd(STDERR_FILENO);

    // If we have nothing to write we can elide the thread.
    // TODO: support eliding output to /dev/null.
    if (f->skip_out() && f->skip_err()) {
        f->internal_proc->mark_exited(p->status);
        return;
    }

    // Ensure that ios stays alive, it may own fds.
    f->ios = ios;

    // If our process is a builtin, it will have already set its status value. Make sure we
    // propagate that if our I/O succeeds and don't read it on a background thread. TODO: have
    // builtin_run provide this directly, rather than setting it in the process.
    f->success_status = p->status;

    iothread_perform_cantwait([f]() {
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
}

/// If \p outdata or \p errdata are both empty, then mark the process as completed immediately.
/// Otherwise, run an internal process.
static void run_internal_process_or_short_circuit(parser_t &parser, const std::shared_ptr<job_t> &j,
                                                  process_t *p, std::string &&outdata,
                                                  std::string &&errdata, const io_chain_t &ios) {
    if (outdata.empty() && errdata.empty()) {
        p->completed = true;
        if (p->is_last_in_job) {
            FLOGF(exec_job_status, L"Set status of job %d (%ls) to %d using short circuit",
                  j->job_id(), j->preview().c_str(), p->status);
            parser.set_last_statuses(j->get_statuses());
        }
    } else {
        run_internal_process(p, std::move(outdata), std::move(errdata), ios);
    }
}

bool blocked_signals_for_job(const job_t &job, sigset_t *sigmask) {
    // Block some signals in background jobs for which job control is turned off (#6828).
    if (!job.is_foreground() && !job.wants_job_control()) {
        sigaddset(sigmask, SIGINT);
        sigaddset(sigmask, SIGQUIT);
        return true;
    }
    return false;
}

/// Call fork() as part of executing a process \p p in a job \j. Execute \p child_action in the
/// context of the child. Returns true if fork succeeded, false if fork failed.
static bool fork_child_for_process(const std::shared_ptr<job_t> &job, process_t *p,
                                   const dup2_list_t &dup2s, const char *fork_type,
                                   const std::function<void()> &child_action) {
    pid_t pid = execute_fork();
    if (pid == 0) {
        // This is the child process. Setup redirections, print correct output to
        // stdout and stderr, and then exit.
        maybe_t<pid_t> new_termowner{};
        p->pid = getpid();
        child_set_group(job.get(), p);
        child_setup_process(job->should_claim_terminal() ? job->pgid : INVALID_PID, *job, true,
                            dup2s);
        child_action();
        DIE("Child process returned control to fork_child lambda!");
    }

    if (pid < 0) {
        FLOGF(warning, L"Failed to fork %s!\n", fork_type);
        job_mark_process_as_failed(job, p);
        return false;
    }

    // This is the parent process. Store away information on the child, and
    // possibly give it control over the terminal.
    s_fork_count++;
    FLOGF(exec_fork, L"Fork #%d, pid %d: %s for '%ls'", int(s_fork_count), pid, fork_type,
          p->argv0());

    p->pid = pid;
    maybe_assign_pgid_from_child(job, p->pid);
    set_child_group(job.get(), p->pid);
    terminal_maybe_give_to_job(job.get(), false);
    return true;
}

/// Execute an internal builtin. Given a parser and a builtin process, execute the builtin with the
/// given streams. If pipe_read is set, assign stdin to it; otherwise infer stdin from the IO chain.
/// \return true on success, false if there is an exec error.
static bool exec_internal_builtin_proc(parser_t &parser, process_t *p, const io_pipe_t *pipe_read,
                                       const io_chain_t &proc_io_chain, io_streams_t &streams) {
    assert(p->type == process_type_t::builtin && "Process must be a builtin");
    int local_builtin_stdin = STDIN_FILENO;
    autoclose_fd_t locally_opened_stdin{};

    // If this is the first process, check the io redirections and see where we should
    // be reading from.
    if (pipe_read) {
        local_builtin_stdin = pipe_read->source_fd;
    } else if (const auto in = proc_io_chain.io_for_fd(STDIN_FILENO)) {
        // Ignore fd redirections from an fd other than the
        // standard ones. e.g. in source <&3 don't actually read from fd 3,
        // which is internal to fish. We still respect this redirection in
        // that we pass it on as a block IO to the code that source runs,
        // and therefore this is not an error.
        bool ignore_redirect =
            in->io_mode == io_mode_t::fd && in->source_fd >= 0 && in->source_fd < 3;
        if (!ignore_redirect) {
            local_builtin_stdin = in->source_fd;
        }
    }

    if (local_builtin_stdin == -1) return false;

    // Determine if we have a "direct" redirection for stdin.
    bool stdin_is_directly_redirected = false;
    if (!p->is_first_in_job) {
        // We must have a pipe
        stdin_is_directly_redirected = true;
    } else {
        // We are not a pipe. Check if there is a redirection local to the process
        // that's not io_mode_t::close.
        for (const auto &redir : p->redirection_specs()) {
            if (redir.fd == STDIN_FILENO && !redir.is_close()) {
                stdin_is_directly_redirected = true;
                break;
            }
        }
    }

    // Pull out the IOs for stdout and stderr.
    auto out_io = proc_io_chain.io_for_fd(STDOUT_FILENO);
    auto err_io = proc_io_chain.io_for_fd(STDERR_FILENO);

    // Set up our streams.
    streams.stdin_fd = local_builtin_stdin;
    streams.out_is_redirected = out_io != nullptr;
    streams.err_is_redirected = err_io != nullptr;
    streams.out_is_piped = (out_io != nullptr && out_io->io_mode == io_mode_t::pipe);
    streams.err_is_piped = (err_io != nullptr && err_io->io_mode == io_mode_t::pipe);
    streams.stdin_is_directly_redirected = stdin_is_directly_redirected;
    streams.io_chain = &proc_io_chain;

    // Note this call may block for a long time, while the builtin performs I/O.
    p->status = builtin_run(parser, p->get_argv(), streams);
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

    const shared_ptr<const io_data_t> stdout_io = io_chain->io_for_fd(STDOUT_FILENO);
    const shared_ptr<const io_data_t> stderr_io = io_chain->io_for_fd(STDERR_FILENO);

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
        auto stdout_buffer = dynamic_cast<const io_bufferfill_t *>(stdout_io.get())->buffer();
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

    // Some historical behavior.
    if (!outbuff.empty()) fflush(stdout);
    if (!errbuff.empty()) fflush(stderr);

    // Construct and run our background process.
    run_internal_process_or_short_circuit(parser, j, p, std::move(outbuff), std::move(errbuff),
                                          *io_chain);
    return true;
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
    bool use_posix_spawn = g_use_posix_spawn && can_use_posix_spawn_for_job(j, dup2s);
    if (use_posix_spawn) {
        s_fork_count++;  // spawn counts as a fork+exec
        // Create posix spawn attributes and actions.
        pid_t pid = 0;
        posix_spawnattr_t attr = posix_spawnattr_t();
        posix_spawn_file_actions_t actions = posix_spawn_file_actions_t();
        bool made_it = fork_actions_make_spawn_properties(&attr, &actions, j.get(), dup2s);
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
        maybe_assign_pgid_from_child(j, p->pid);

        // In glibc, posix_spawn uses fork() and the pgid group is set on the child side;
        // therefore the parent may not have seen it be set yet.
        // Ensure it gets set. See #4715, also https://github.com/Microsoft/WSL/issues/2997.
        set_child_group(j.get(), p->pid);
        terminal_maybe_give_to_job(j.get(), false);
    } else
#endif
    {
        if (!fork_child_for_process(j, p, dup2s, "external command",
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

// The "performer" function of a block or function process.
// This accepts a place to execute as \p parser, and a parent job as \p parent, and then executes
// the result, returning a status.
// This is factored out in this funny way in preparation for concurrent execution.
using proc_performer_t = std::function<proc_status_t(parser_t &parser)>;

// \return a function which may be to run the given process \p.
// May return an empty std::function in the rare case that the to-be called fish function no longer
// exists. This is just a dumb artifact of the fact that we only capture the functions name, not its
// properties, when creating the job; thus a race could delete the function before we fetch its
// properties.
static proc_performer_t get_performer_for_process(process_t *p, job_t *job,
                                                  const io_chain_t &io_chain) {
    assert((p->type == process_type_t::function || p->type == process_type_t::block_node) &&
           "Unexpected process type");
    // Make a lineage for our children.
    job_lineage_t lineage;
    lineage.parent_pgid = (job->pgid == INVALID_PID ? none() : maybe_t<pid_t>(job->pgid));
    lineage.block_io = io_chain;
    lineage.root_constructed = job->root_constructed;
    lineage.root_has_job_control = job->wants_job_control();

    if (p->type == process_type_t::block_node) {
        const parsed_source_ref_t &source = p->block_node_source;
        tnode_t<grammar::statement> node = p->internal_block_node;
        assert(source && node && "Process is missing node info");
        return [=](parser_t &parser) { return parser.eval_node(source, node, lineage).status; };
    } else {
        assert(p->type == process_type_t::function);
        auto props = function_get_properties(p->argv0());
        if (!props) {
            FLOGF(error, _(L"Unknown function '%ls'"), p->argv0());
            return proc_performer_t{};
        }
        auto argv = move_to_sharedptr(p->get_argv_array().to_list());
        return [=](parser_t &parser) {
            // Pull out the job list from the function.
            tnode_t<grammar::job_list> body = props->func_node.child<1>();
            const block_t *fb = function_prepare_environment(parser, *argv, *props);
            auto res = parser.eval_node(props->parsed_source, body, lineage);
            function_restore_environment(parser, fb);

            // If the function did not execute anything, treat it as success.
            if (res.was_empty) {
                res = proc_status_t::from_exit_code(EXIT_SUCCESS);
            }
            return res.status;
        };
    }
}

/// Execute a block node or function "process".
/// \p conflicts contains the list of fds which pipes should avoid.
/// \p allow_buffering if true, permit buffering the output.
/// \return true on success, false on error.
static bool exec_block_or_func_process(parser_t &parser, const std::shared_ptr<job_t> &j,
                                       process_t *p, const fd_set_t &conflicts, io_chain_t io_chain,
                                       bool allow_buffering) {
    // Create an output buffer if we're piping to another process.
    shared_ptr<io_bufferfill_t> block_output_bufferfill{};
    if (!p->is_last_in_job && allow_buffering) {
        // Be careful to handle failure, e.g. too many open fds.
        block_output_bufferfill = io_bufferfill_t::create(conflicts);
        if (!block_output_bufferfill) {
            job_mark_process_as_failed(j, p);
            return false;
        }
        // Teach the job about its bufferfill, and add it to our io chain.
        io_chain.push_back(block_output_bufferfill);
    }

    // If this function is the only process in the current job
    // and that job is in the foreground then mark this job as internal
    // so it doesn't increment the job id for any jobs created within this
    // function.
    if (p->is_first_in_job && p->is_last_in_job && j->flags().foreground) {
        j->mark_internal();
    }

    // Get the process performer, and just execute it directly.
    // Do it in this scoped way so that the performer function can be eagerly deallocating releasing
    // its captured io chain.
    if (proc_performer_t performer = get_performer_for_process(p, j.get(), io_chain)) {
        p->status = performer(parser);
    } else {
        return false;
    }

    // If we have a block output buffer, populate it now.
    std::string buffer_contents;
    if (block_output_bufferfill) {
        // Remove our write pipe and forget it. This may close the pipe, unless another thread has
        // claimed it (background write) or another process has inherited it.
        io_chain.remove(block_output_bufferfill);
        auto block_output_buffer = io_bufferfill_t::finish(std::move(block_output_bufferfill));
        buffer_contents = block_output_buffer->buffer().newline_serialized();
    }

    run_internal_process_or_short_circuit(parser, j, p, std::move(buffer_contents),
                                          {} /* errdata */, io_chain);
    return true;
}

/// Executes a process \p \p in \p job, using the pipes \p pipes (which may have invalid fds if this
/// is the first or last process).
/// \p deferred_pipes represents the pipes from our deferred process; if set ensure they get closed
/// in any child. If \p is_deferred_run is true, then this is a deferred run; this affects how
/// certain buffering works. \returns true on success, false on exec error.
static bool exec_process_in_job(parser_t &parser, process_t *p, const std::shared_ptr<job_t> &j,
                                const io_chain_t &block_io, autoclose_pipes_t pipes,
                                const fd_set_t &conflicts, const autoclose_pipes_t &deferred_pipes,
                                size_t stdout_read_limit, bool is_deferred_run = false) {
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
    io_chain_t process_net_io_chain = block_io;

    if (pipes.write.valid()) {
        process_net_io_chain.push_back(std::make_shared<io_pipe_t>(
            p->pipe_write_fd, false /* not input */, std::move(pipes.write)));
    }

    // Append IOs from the process's redirection specs.
    // This may fail.
    if (!process_net_io_chain.append_from_specs(p->redirection_specs(),
                                                parser.vars().get_pwd_slash())) {
        // If the redirection to a file failed, append_from_specs closes the corresponding handle
        // allowing us to recover from failure mid-way through execution of a piped job to e.g.
        // recover from loss of terminal control, etc.

        // It is unsafe to abort execution in the middle of a multi-command job as we might end up
        // trying to resume execution without control of the terminal.
        if (p->is_first_in_job) {
            // It's OK to abort here
            return false;
        }
        // Otherwise, just rely on the closed fd to take care of things. It's also probably more
        // semantically correct!
    }

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
        // A simple `begin ... end` should not be considered an execution of a command.
        parser.libdata().exec_count++;
    }

    const block_t *block = nullptr;
    cleanup_t pop_block([&]() {
        if (block) parser.pop_block(block);
    });
    if (!p->variable_assignments.empty()) {
        block = parser.push_block(block_t::variable_assignment_block());
    }
    for (const auto &assignment : p->variable_assignments) {
        parser.vars().set(assignment.variable_name, ENV_LOCAL | ENV_EXPORT, assignment.values);
    }

    // Execute the process.
    p->check_generations_before_launch();
    switch (p->type) {
        case process_type_t::function:
        case process_type_t::block_node: {
            // Allow buffering unless this is a deferred run. If deferred, then processes after us
            // were already launched, so they are ready to receive (or reject) our output.
            bool allow_buffering = !is_deferred_run;
            if (!exec_block_or_func_process(parser, j, p, conflicts, process_net_io_chain,
                                            allow_buffering)) {
                return false;
            }
            break;
        }

        case process_type_t::builtin: {
            io_streams_t builtin_io_streams{stdout_read_limit};
            if (j->pgid != INVALID_PID) builtin_io_streams.parent_pgid = j->pgid;
            if (!exec_internal_builtin_proc(parser, p, pipe_read.get(), process_net_io_chain,
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

// Given that we are about to execute an exec() call, check if the parser is interactive and there
// are extant background jobs. If so, warn the user and do not exec().
// \return true if we should allow exec, false to disallow it.
static bool allow_exec_with_background_jobs(parser_t &parser) {
    // If we're not interactive, we cannot warn.
    if (!parser.is_interactive()) return true;

    // Construct the list of running background jobs.
    job_list_t bgs = jobs_requiring_warning_on_exit(parser);
    if (bgs.empty()) return true;

    // Compare run counts, so we only warn once.
    uint64_t current_run_count = reader_run_count();
    uint64_t &last_exec_run_count = parser.libdata().last_exec_run_counter;
    if (isatty(STDIN_FILENO) && current_run_count - 1 != last_exec_run_count) {
        print_exit_warning_for_jobs(bgs);
        last_exec_run_count = current_run_count;
        return false;
    } else {
        hup_jobs(parser.jobs());
        return true;
    }
}

bool exec_job(parser_t &parser, const shared_ptr<job_t> &j, const job_lineage_t &lineage) {
    assert(j && "null job_t passed to exec_job!");

    // Set to true if something goes wrong while executing the job, in which case the cleanup
    // code will kick in.
    bool exec_error = false;

    // If fish was invoked with -n or --no-execute, then no_exec will be set and we do nothing.
    if (no_exec()) {
        return true;
    }

    pid_t pgrp = getpgrp();
    // Check to see if we should reclaim the foreground pgrp after the job finishes or stops.
    const bool reclaim_foreground_pgrp = (tcgetpgrp(STDIN_FILENO) == pgrp);

    // Perhaps we know our pgroup already.
    assert(j->pgid == INVALID_PID && "Should not yet have a pid.");
    switch (j->pgroup_provenance) {
        case pgroup_provenance_t::lineage:
            assert(*lineage.parent_pgid != INVALID_PID && "pgid should be none, not INVALID_PID");
            j->pgid = *lineage.parent_pgid;
            break;

        case pgroup_provenance_t::fish_itself:
            j->pgid = pgrp;
            break;

        // The remaining cases are all deferred until later.
        case pgroup_provenance_t::unassigned:
        case pgroup_provenance_t::first_external_proc:
            break;
    }

    const size_t stdout_read_limit = parser.libdata().read_limit;

    // Get the list of all FDs so we can ensure our pipes do not conflict.
    fd_set_t conflicts = lineage.block_io.fd_set();
    for (const auto &p : j->processes) {
        for (const auto &spec : p->redirection_specs()) {
            conflicts.add(spec.fd);
        }
    }

    // Handle an exec call.
    if (j->processes.front()->type == process_type_t::exec) {
        // If we are interactive, perhaps disallow exec if there are background jobs.
        if (!allow_exec_with_background_jobs(parser)) {
            return false;
        }

        internal_exec(parser.vars(), j.get(), lineage.block_io);
        // internal_exec only returns if it failed to set up redirections.
        // In case of an successful exec, this code is not reached.
        int status = j->flags().negate ? 0 : 1;
        parser.set_last_statuses(statuses_t::just(status));

        // A false return tells the caller to remove the job from the list.
        return false;
    }
    cleanup_t timer = push_timer(j->flags().has_time_prefix && !no_exec());

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
            auto pipes = make_autoclose_pipes(conflicts);
            if (!pipes) {
                FLOGF(warning, PIPE_ERROR);
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
            if (!exec_process_in_job(parser, p.get(), j, lineage.block_io, std::move(proc_pipes),
                                     conflicts, deferred_pipes, stdout_read_limit)) {
                exec_error = true;
                break;
            }
        }
    }
    pipe_next_read.close();

    // Now execute any deferred process.
    if (!exec_error && deferred_process) {
        assert(deferred_pipes.write.valid() && "Deferred process should always have a write pipe");
        if (!exec_process_in_job(parser, deferred_process, j, lineage.block_io,
                                 std::move(deferred_pipes), conflicts, {}, stdout_read_limit,
                                 true)) {
            exec_error = true;
        }
    }

    FLOGF(exec_job_exec, L"Executed job %d from command '%ls' with pgrp %d", j->job_id(),
          j->command_wcstr(), j->pgid);

    j->mark_constructed();
    if (!j->is_foreground()) {
        parser.vars().set_one(L"last_pid", ENV_GLOBAL, to_string(j->pgid));
    }

    if (exec_error) {
        return false;
    }

    j->continue_job(parser, reclaim_foreground_pgrp, false);
    return true;
}

/// Populate \p lst with the output of \p buffer, perhaps splitting lines according to \p split.
static void populate_subshell_output(wcstring_list_t *lst, const io_buffer_t &buffer, bool split) {
    // Walk over all the elements.
    for (const auto &elem : buffer.buffer().elements()) {
        if (elem.is_explicitly_separated()) {
            // Just append this one.
            lst->push_back(str2wcstring(elem.contents));
            continue;
        }

        // Not explicitly separated. We have to split it explicitly.
        assert(!elem.is_explicitly_separated() && "should not be explicitly separated");
        const char *begin = elem.contents.data();
        const char *end = begin + elem.contents.size();
        if (split) {
            const char *cursor = begin;
            while (cursor < end) {
                // Look for the next separator.
                auto stop = static_cast<const char *>(std::memchr(cursor, '\n', end - cursor));
                const bool hit_separator = (stop != nullptr);
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
}

/// Execute \p cmd in a subshell in \p parser. If \p lst is not null, populate it with the output.
/// Return $status in \p out_status.
/// If \p parent_pgid is set, any spawned commands should join that pgroup.
/// If \p apply_exit_status is false, then reset $status back to its original value.
/// \p is_subcmd controls whether we apply a read limit.
/// \p break_expand is used to propagate whether the result should be "expansion breaking" in the
/// sense that subshells used during string expansion should halt that expansion. \return the value
/// of $status.
static int exec_subshell_internal(const wcstring &cmd, parser_t &parser, maybe_t<pid_t> parent_pgid,
                                  wcstring_list_t *lst, bool *break_expand, bool apply_exit_status,
                                  bool is_subcmd) {
    ASSERT_IS_MAIN_THREAD();
    auto &ld = parser.libdata();

    scoped_push<bool> is_subshell(&ld.is_subshell, true);
    scoped_push<size_t> read_limit(&ld.read_limit, is_subcmd ? read_byte_limit : 0);

    auto prev_statuses = parser.get_last_statuses();
    const cleanup_t put_back([&] {
        if (!apply_exit_status) {
            parser.set_last_statuses(prev_statuses);
        }
    });

    const bool split_output = !parser.vars().get(L"IFS").missing_or_empty();

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may
    // be null.
    auto bufferfill = io_bufferfill_t::create(fd_set_t{}, ld.read_limit);
    if (!bufferfill) {
        *break_expand = true;
        return STATUS_CMD_ERROR;
    }
    eval_res_t eval_res =
        parser.eval(cmd, io_chain_t{bufferfill}, parent_pgid, block_type_t::subst);
    std::shared_ptr<io_buffer_t> buffer = io_bufferfill_t::finish(std::move(bufferfill));
    if (buffer->buffer().discarded()) {
        *break_expand = true;
        return STATUS_READ_TOO_MUCH;
    }

    if (eval_res.break_expand) {
        *break_expand = true;
        return eval_res.status.status_value();
    }

    if (lst) {
        populate_subshell_output(lst, *buffer, split_output);
    }
    *break_expand = false;
    return eval_res.status.status_value();
}

int exec_subshell_for_expand(const wcstring &cmd, parser_t &parser, maybe_t<pid_t> parent_pgid,
                             wcstring_list_t &outputs) {
    ASSERT_IS_MAIN_THREAD();
    bool break_expand = false;
    int ret = exec_subshell_internal(cmd, parser, parent_pgid, &outputs, &break_expand, true, true);
    // Only return an error code if we should break expansion.
    return break_expand ? ret : STATUS_CMD_OK;
}

int exec_subshell(const wcstring &cmd, parser_t &parser, bool apply_exit_status) {
    bool break_expand = false;
    return exec_subshell_internal(cmd, parser, none(), nullptr, &break_expand, apply_exit_status,
                                  false);
}

int exec_subshell(const wcstring &cmd, parser_t &parser, wcstring_list_t &outputs,
                  bool apply_exit_status) {
    bool break_expand = false;
    return exec_subshell_internal(cmd, parser, none(), &outputs, &break_expand, apply_exit_status,
                                  false);
}
