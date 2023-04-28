#if 0
// Functions for executing a program.
//
// Some of the code in this file is based on code from the Glibc manual, though the changes
// performed have been massive.
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "trace.rs.h"
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <paths.h>
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "builtin.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fds.h"
#include "ffi.h"
#include "flog.h"
#include "function.h"
#include "global_safety.h"
#include "io.h"
#include "iothread.h"
#include "job_group.rs.h"
#include "maybe.h"
#include "null_terminated_array.h"
#include "parse_tree.h"
#include "parser.h"
#include "postfork.h"
#include "proc.h"
#include "reader.h"
#include "redirection.h"
#include "timer.rs.h"
#include "trace.rs.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

/// Number of calls to fork() or posix_spawn().
static relaxed_atomic_t<int> s_fork_count{0};

/// A launch_result_t indicates when a process failed to launch, and therefore the rest of the
/// pipeline should be aborted. This includes failed redirections, fd exhaustion, fork() failures,
/// etc.
enum class launch_result_t {
    ok,
    failed,
} __warn_unused_type;

/// Given an error \p err returned from either posix_spawn or exec, \return a process exit code.
static int exit_code_from_exec_error(int err) {
    assert(err && "Zero is success, not an error");
    switch (err) {
        case ENOENT:
        case ENOTDIR:
            // This indicates either the command was not found, or a file redirection was not found.
            // We do not use posix_spawn file redirections so this is always command-not-found.
            return STATUS_CMD_UNKNOWN;

        case EACCES:
        case ENOEXEC:
            // The file is not executable for various reasons.
            return STATUS_NOT_EXECUTABLE;

#ifdef EBADARCH
        case EBADARCH:
            // This is for e.g. running ARM app on Intel Mac.
            return STATUS_NOT_EXECUTABLE;
#endif
        default:
            // Generic failure.
            return EXIT_FAILURE;
    }
}

/// This is a 'looks like text' check.
/// \return true if either there is no NUL byte, or there is a line containing a lowercase letter
/// before the first NUL byte.
static bool is_thompson_shell_payload(const char *p, size_t n) {
    if (!memchr(p, '\0', n)) return true;
    bool haslower = false;
    for (; *p; p++) {
        if (islower(*p) || *p == '$' || *p == '`') {
            haslower = true;
        }
        if (haslower && *p == '\n') {
            return true;
        }
    }
    return false;
}

/// This function checks the beginning of a file to see if it's safe to
/// pass to the system interpreter when execve() returns ENOEXEC.
///
/// The motivation is to be able to run classic shell scripts which
/// didn't have shebang, while protecting the user from accidentally
/// running a binary file which may corrupt terminal driver state. We
/// check for lowercase letters because the ASCII magic of binary files
/// is usually uppercase, e.g. PNG, JFIF, MZ, etc. These rules are also
/// flexible enough to permit scripts with concatenated binary content,
/// such as Actually Portable Executable.
/// N.B.: this is called after fork, it must not allocate heap memory.
bool is_thompson_shell_script(const char *path) {
    // Paths ending in ".fish" are never considered Thompson shell scripts.
    if (const char *lastdot = strrchr(path, '.')) {
        if (0 == strcmp(lastdot, ".fish")) {
            return false;
        }
    }
    int e = errno;
    bool res = false;
    int fd = open_cloexec(path, O_RDONLY | O_NOCTTY);
    if (fd != -1) {
        char buf[256];
        ssize_t got = read(fd, buf, sizeof(buf));
        close(fd);
        if (got >= 0 && is_thompson_shell_payload(buf, static_cast<size_t>(got))) {
            res = true;
        }
    }
    errno = e;
    return res;
}

/// This function is executed by the child process created by a call to fork(). It should be called
/// after \c child_setup_process. It calls execve to replace the fish process image with the command
/// specified in \c p. It never returns. Called in a forked child! Do not allocate memory, etc.
[[noreturn]] static void safe_launch_process(process_t *p, const char *actual_cmd,
                                             const char *const *cargv, const char *const *cenvv) {
    UNUSED(p);
    int err;

    // This function never returns, so we take certain liberties with constness.
    auto envv = const_cast<char **>(cenvv);
    auto argv = const_cast<char **>(cargv);
    auto cmd2 = const_cast<char *>(actual_cmd);

    execve(actual_cmd, argv, envv);
    err = errno;

    // The shebang wasn't introduced until UNIX Seventh Edition, so if
    // the kernel won't run the binary we hand it off to the interpreter
    // after performing a binary safety check, recommended by POSIX: a
    // line needs to exist before the first \0 with a lowercase letter
    if (err == ENOEXEC && is_thompson_shell_script(actual_cmd)) {
        // Construct new argv.
        // We must not allocate memory, so only 128 args are supported.
        constexpr size_t maxargs = 128;
        size_t nargs = 0;
        while (argv[nargs]) nargs++;
        if (nargs <= maxargs) {
            char *argv2[1 + maxargs + 1];  // +1 for /bin/sh, +1 for terminating nullptr
            char interp[] = _PATH_BSHELL;
            argv2[0] = interp;
            std::copy_n(argv, 1 + nargs, &argv2[1]);  // +1 to copy terminating nullptr
            // The command to call should use the full path,
            // not what we would pass as argv0.
            argv2[1] = cmd2;
            execve(_PATH_BSHELL, argv2, envv);
        }
    }

    errno = err;
    safe_report_exec_error(errno, actual_cmd, argv, envv);
    exit_without_destructors(exit_code_from_exec_error(err));
}

/// This function is similar to launch_process, except it is not called after a fork (i.e. it only
/// calls exec) and therefore it can allocate memory.
[[noreturn]] static void launch_process_nofork(env_stack_t &vars, process_t *p) {
    ASSERT_IS_NOT_FORKED_CHILD();

    // Construct argv. Ensure the strings stay alive for the duration of this function.
    std::vector<std::string> narrow_strings = wide_string_list_to_narrow(p->argv());
    null_terminated_array_t<char> narrow_argv(narrow_strings);
    const char **argv = narrow_argv.get();

    // Construct envp.
    auto export_vars = vars.export_arr();
    const char **envp = export_vars->get();
    std::string actual_cmd = wcs2zstring(p->actual_cmd);

    // Ensure the terminal modes are what they were before we changed them.
    restore_term_mode();
    // Bounce to launch_process. This never returns.
    safe_launch_process(p, actual_cmd.c_str(), argv, envp);
}

// Returns whether we can use posix spawn for a given process in a given job.
//
// To avoid the race between the caller calling tcsetpgrp() and the client checking the
// foreground process group, we don't use posix_spawn if we're going to foreground the process. (If
// we use fork(), we can call tcsetpgrp after the fork, before the exec, and avoid the race).
static bool can_use_posix_spawn_for_job(const std::shared_ptr<job_t> &job,
                                        const dup2_list_t &dup2s) {
    // Is it globally disabled?
    if (!get_use_posix_spawn()) return false;

    // Hack - do not use posix_spawn if there are self-fd redirections.
    // For example if you were to write:
    //   cmd 6< /dev/null
    // it is possible that the open() of /dev/null would result in fd 6. Here even if we attempted
    // to add a dup2 action, it would be ignored and the CLO_EXEC bit would remain. So don't use
    // posix_spawn in this case; instead we'll call fork() and clear the CLO_EXEC bit manually.
    for (const auto &action : dup2s.get_actions()) {
        if (action.src == action.target) return false;
    }
    if (job->group->wants_terminal()) {
        // This job will be foregrounded, so we will call tcsetpgrp(), therefore do not use
        // posix_spawn.
        return false;
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
    dup2_list_t redirs = dup2_list_resolve_chain_shim(all_ios);
    if (child_setup_process(false /* not claim_tty */, *j, false /* not is_forked */, redirs) ==
        0) {
        // Decrement SHLVL as we're removing ourselves from the shell "stack".
        if (is_interactive_session()) {
            auto shlvl_var = vars.get(L"SHLVL", ENV_GLOBAL | ENV_EXPORT);
            wcstring shlvl_str = L"0";
            if (shlvl_var) {
                long shlvl = fish_wcstol(shlvl_var->as_string().c_str());
                if (!errno && shlvl > 0) {
                    shlvl_str = to_string(shlvl - 1);
                }
            }
            vars.set_one(L"SHLVL", ENV_GLOBAL | ENV_EXPORT, std::move(shlvl_str));
        }

        // launch_process _never_ returns.
        launch_process_nofork(vars, p);
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
    f->dup2s = dup2_list_resolve_chain_shim(ios);

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
            auto statuses = j->get_statuses();
            if (statuses) {
                parser.set_last_statuses(statuses.value());
                parser.libdata().status_count++;
            } else if (j->flags().negate) {
                // Special handling for `not set var (substitution)`.
                // If there is no status, but negation was requested,
                // take the last status and negate it.
                auto last_statuses = parser.get_last_statuses();
                last_statuses.status = !last_statuses.status;
                parser.set_last_statuses(last_statuses);
            }
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
/// context of the child.
static launch_result_t fork_child_for_process(const std::shared_ptr<job_t> &job, process_t *p,
                                              const dup2_list_t &dup2s, const char *fork_type,
                                              const std::function<void()> &child_action) {
    // Claim the tty from fish, if the job wants it and we are the pgroup leader.
    pid_t claim_tty_from =
        (p->leads_pgrp && job->group->wants_terminal()) ? getpgrp() : INVALID_PID;

    pid_t pid = execute_fork();
    if (pid < 0) {
        return launch_result_t::failed;
    }
    const bool is_parent = (pid > 0);

    // Record the pgroup if this is the leader.
    // Both parent and child attempt to send the process to its new group, to resolve the race.
    p->pid = is_parent ? pid : getpid();
    if (p->leads_pgrp) {
        job->group->set_pgid(p->pid);
    }
    {
        auto pgid = job->group->get_pgid();
        if (pgid) {
            if (int err = execute_setpgid(p->pid, pgid->value, is_parent)) {
                report_setpgid_error(err, is_parent, pgid->value, job.get(), p);
            }
        }
    }

    if (!is_parent) {
        // Child process.
        child_setup_process(claim_tty_from, *job, true, dup2s);
        child_action();
        DIE("Child process returned control to fork_child lambda!");
    }

    ++s_fork_count;
    FLOGF(exec_fork, L"Fork #%d, pid %d: %s for '%ls'", int(s_fork_count), pid, fork_type,
          p->argv0());
    return launch_result_t::ok;
}

/// \return an newly allocated output stream for the given fd, which is typically stdout or stderr.
/// This inspects the io_chain and decides what sort of output stream to return.
/// If \p piped_output_needs_buffering is set, and if the output is going to a pipe, then the other
/// end then synchronously writing to the pipe risks deadlock, so we must buffer it.
static std::shared_ptr<output_stream_t> create_output_stream_for_builtin(
    int fd, const io_chain_t &io_chain, bool piped_output_needs_buffering) {
    using std::make_shared;
    const shared_ptr<const io_data_t> io = io_chain.io_for_fd(fd);
    if (io == nullptr) {
        // Common case of no redirections.
        // Just write to the fd directly.
        return make_shared<fd_output_stream_t>(fd);
    }
    switch (io->io_mode) {
        case io_mode_t::bufferfill: {
            // Our IO redirection is to an internal buffer, e.g. a command substitution.
            // We will write directly to it.
            std::shared_ptr<io_buffer_t> buffer =
                std::static_pointer_cast<const io_bufferfill_t>(io)->buffer();
            return make_unique<buffered_output_stream_t>(buffer);
        }

        case io_mode_t::close:
            // Like 'echo foo >&-'
            return make_shared<null_output_stream_t>();

        case io_mode_t::file:
            // Output is to a file which has been opened.
            return make_shared<fd_output_stream_t>(io->source_fd);

        case io_mode_t::pipe:
            // Output is to a pipe. We may need to buffer.
            if (piped_output_needs_buffering) {
                return make_shared<string_output_stream_t>();
            } else {
                return make_shared<fd_output_stream_t>(io->source_fd);
            }

        case io_mode_t::fd:
            // This is a case like 'echo foo >&5'
            // It's uncommon and unclear what should happen.
            return make_shared<string_output_stream_t>();
    }
    DIE("Unreachable");
}

/// Handle output from a builtin, by printing the contents of builtin_io_streams to the redirections
/// given in io_chain.
static void handle_builtin_output(parser_t &parser, const std::shared_ptr<job_t> &j, process_t *p,
                                  const io_chain_t &io_chain, const output_stream_t &out,
                                  const output_stream_t &err) {
    assert(p->type == process_type_t::builtin && "Process is not a builtin");

    // Figure out any data remaining to write. We may have none, in which case we can short-circuit.
    std::string outbuff = wcs2string(out.contents());
    std::string errbuff = wcs2string(err.contents());

    // Some historical behavior.
    if (!outbuff.empty()) fflush(stdout);
    if (!errbuff.empty()) fflush(stderr);

    // Construct and run our background process.
    run_internal_process_or_short_circuit(parser, j, p, std::move(outbuff), std::move(errbuff),
                                          io_chain);
}

/// Executes an external command.
/// An error return here indicates that the process failed to launch, and the rest of
/// the pipeline should be cancelled.
static launch_result_t exec_external_command(parser_t &parser, const std::shared_ptr<job_t> &j,
                                             process_t *p, const io_chain_t &proc_io_chain) {
    assert(p->type == process_type_t::external && "Process is not external");
    // Get argv and envv before we fork.
    const std::vector<std::string> narrow_argv = wide_string_list_to_narrow(p->argv());
    null_terminated_array_t<char> argv_array(narrow_argv);

    // Convert our IO chain to a dup2 sequence.
    auto dup2s = dup2_list_resolve_chain_shim(proc_io_chain);

    // Ensure that stdin is blocking before we hand it off (see issue #176).
    // Note this will also affect stdout and stderr if they refer to the same tty.
    make_fd_blocking(STDIN_FILENO);

    auto export_arr = parser.vars().export_arr();
    const char *const *argv = argv_array.get();
    const char *const *envv = export_arr->get();

    std::string actual_cmd_str = wcs2zstring(p->actual_cmd);
    const char *actual_cmd = actual_cmd_str.c_str();
    filename_ref_t file = parser.libdata().current_filename;

#if FISH_USE_POSIX_SPAWN
    // Prefer to use posix_spawn, since it's faster on some systems like OS X.
    if (can_use_posix_spawn_for_job(j, dup2s)) {
        ++s_fork_count;  // spawn counts as a fork+exec

        posix_spawner_t spawner(j.get(), dup2s);
        maybe_t<pid_t> pid = spawner.spawn(actual_cmd, const_cast<char *const *>(argv),
                                           const_cast<char *const *>(envv));
        if (int err = spawner.get_error()) {
            safe_report_exec_error(err, actual_cmd, argv, envv);
            p->status = proc_status_t::from_exit_code(exit_code_from_exec_error(err));
            return launch_result_t::failed;
        }
        assert(pid.has_value() && *pid > 0 && "Should have either a valid pid, or an error");

        // This usleep can be used to test for various race conditions
        // (https://github.com/fish-shell/fish-shell/issues/360).
        // usleep(10000);

        FLOGF(exec_fork, L"Fork #%d, pid %d: spawn external command '%s' from '%ls'",
              int(s_fork_count), *pid, actual_cmd, file ? file->c_str() : L"<no file>");

        // these are all things do_fork() takes care of normally (for forked processes):
        p->pid = *pid;
        if (p->leads_pgrp) {
            j->group->set_pgid(p->pid);
            // posix_spawn should in principle set the pgid before returning.
            // In glibc, posix_spawn uses fork() and the pgid group is set on the child side;
            // therefore the parent may not have seen it be set yet.
            // Ensure it gets set. See #4715, also https://github.com/Microsoft/WSL/issues/2997.
            execute_setpgid(p->pid, p->pid, true /* is parent */);
        }
        return launch_result_t::ok;
    } else
#endif
    {
        return fork_child_for_process(j, p, dup2s, "external command",
                                      [&] { safe_launch_process(p, actual_cmd, argv, envv); });
    }
}

// Given that we are about to execute a function, push a function block and set up the
// variable environment.
static block_t *function_prepare_environment(parser_t &parser, std::vector<wcstring> argv,
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
// This accepts a place to execute as \p parser and then executes the result, returning a status.
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
    // We want to capture the job group.
    job_group_ref_t job_group = job->group;

    if (p->type == process_type_t::block_node) {
        const parsed_source_ref_t &source = *p->block_node_source;
        const ast::statement_t *node = p->internal_block_node;
        assert(source.has_value() && node && "Process is missing node info");
        // The lambda will convert into a std::function which requires copyability. A Box can't
        // be copied, so add another indirection.
        auto source_box = std::make_shared<rust::Box<ParsedSourceRefFFI>>(source.clone());
        return [=](parser_t &parser) {
            return parser.eval_node(**source_box, *node, io_chain, job_group).status;
        };
    } else {
        assert(p->type == process_type_t::function);
        auto props = function_get_props(p->argv0());
        if (!props) {
            FLOGF(error, _(L"Unknown function '%ls'"), p->argv0());
            return proc_performer_t{};
        }
        const std::vector<wcstring> &argv = p->argv();
        return [=](parser_t &parser) {
            // Pull out the job list from the function.
            const ast::job_list_t &body = props->func_node->jobs();
            const block_t *fb = function_prepare_environment(parser, argv, *props);
            auto res = parser.eval_node(*props->parsed_source, body, io_chain, job_group);
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
/// \p piped_output_needs_buffering if true, buffer the output.
static launch_result_t exec_block_or_func_process(parser_t &parser, const std::shared_ptr<job_t> &j,
                                                  process_t *p, io_chain_t io_chain,
                                                  bool piped_output_needs_buffering) {
    // Create an output buffer if we're piping to another process.
    shared_ptr<io_bufferfill_t> block_output_bufferfill{};
    if (piped_output_needs_buffering) {
        // Be careful to handle failure, e.g. too many open fds.
        block_output_bufferfill = io_bufferfill_t::create();
        if (!block_output_bufferfill) {
            return launch_result_t::failed;
        }
        // Teach the job about its bufferfill, and add it to our io chain.
        io_chain.push_back(block_output_bufferfill);
    }

    // Get the process performer, and just execute it directly.
    // Do it in this scoped way so that the performer function can be eagerly deallocating releasing
    // its captured io chain.
    if (proc_performer_t performer = get_performer_for_process(p, j.get(), io_chain)) {
        p->status = performer(parser);
    } else {
        return launch_result_t::failed;
    }

    // If we have a block output buffer, populate it now.
    std::string buffer_contents;
    if (block_output_bufferfill) {
        // Remove our write pipe and forget it. This may close the pipe, unless another thread has
        // claimed it (background write) or another process has inherited it.
        io_chain.remove(block_output_bufferfill);
        buffer_contents =
            io_bufferfill_t::finish(std::move(block_output_bufferfill)).newline_serialized();
    }

    run_internal_process_or_short_circuit(parser, j, p, std::move(buffer_contents),
                                          {} /* errdata */, io_chain);
    return launch_result_t::ok;
}

static proc_performer_t get_performer_for_builtin(
    process_t *p, job_t *job, const io_chain_t &io_chain,
    const std::shared_ptr<output_stream_t> &output_stream,
    const std::shared_ptr<output_stream_t> &errput_stream) {
    assert(p->type == process_type_t::builtin && "Process must be a builtin");

    // Determine if we have a "direct" redirection for stdin.
    bool stdin_is_directly_redirected = false;
    if (!p->is_first_in_job) {
        // We must have a pipe
        stdin_is_directly_redirected = true;
    } else {
        // We are not a pipe. Check if there is a redirection local to the process
        // that's not io_mode_t::close.
        for (size_t i = 0; i < p->redirection_specs().size(); i++) {
            const auto *redir = p->redirection_specs().at(i);
            if (redir->fd() == STDIN_FILENO && !redir->is_close()) {
                stdin_is_directly_redirected = true;
                break;
            }
        }
    }

    // Pull out some fields which we want to copy. We don't want to store the process or job in the
    // returned closure.
    job_group_ref_t job_group = job->group;
    const std::vector<wcstring> &argv = p->argv();

    // Be careful to not capture p or j by value, as the intent is that this may be run on another
    // thread.
    return [=](parser_t &parser) {
        auto out_io = io_chain.io_for_fd(STDOUT_FILENO);
        auto err_io = io_chain.io_for_fd(STDERR_FILENO);

        // Figure out what fd to use for the builtin's stdin.
        int local_builtin_stdin = STDIN_FILENO;
        if (const auto in = io_chain.io_for_fd(STDIN_FILENO)) {
            // Ignore fd redirections from an fd other than the
            // standard ones. e.g. in source <&3 don't actually read from fd 3,
            // which is internal to fish. We still respect this redirection in
            // that we pass it on as a block IO to the code that source runs,
            // and therefore this is not an error.
            bool ignore_redirect = in->io_mode == io_mode_t::fd && in->source_fd >= 3;
            if (!ignore_redirect) {
                local_builtin_stdin = in->source_fd;
            }
        }

        // Populate our io_streams_t. This is a bag of information for the builtin.
        io_streams_t streams{*output_stream, *errput_stream};
        streams.job_group = job_group;
        streams.stdin_fd = local_builtin_stdin;
        streams.stdin_is_directly_redirected = stdin_is_directly_redirected;
        streams.out_is_redirected = out_io != nullptr;
        streams.err_is_redirected = err_io != nullptr;
        streams.out_is_piped = (out_io && out_io->io_mode == io_mode_t::pipe);
        streams.err_is_piped = (err_io && err_io->io_mode == io_mode_t::pipe);
        streams.io_chain = &io_chain;

        // Execute the builtin.
        return builtin_run(parser, argv, streams);
    };
}

/// Executes a builtin "process".
static launch_result_t exec_builtin_process(parser_t &parser, const std::shared_ptr<job_t> &j,
                                            process_t *p, const io_chain_t &io_chain,
                                            bool piped_output_needs_buffering) {
    assert(p->type == process_type_t::builtin && "Process is not a builtin");
    std::shared_ptr<output_stream_t> out =
        create_output_stream_for_builtin(STDOUT_FILENO, io_chain, piped_output_needs_buffering);
    std::shared_ptr<output_stream_t> err =
        create_output_stream_for_builtin(STDERR_FILENO, io_chain, piped_output_needs_buffering);

    if (proc_performer_t performer = get_performer_for_builtin(p, j.get(), io_chain, out, err)) {
        p->status = performer(parser);
    } else {
        return launch_result_t::failed;
    }
    handle_builtin_output(parser, j, p, io_chain, *out, *err);
    return launch_result_t::ok;
}

/// Executes a process \p \p in \p job, using the pipes \p pipes (which may have invalid fds if this
/// is the first or last process).
/// \p deferred_pipes represents the pipes from our deferred process; if set ensure they get closed
/// in any child. If \p is_deferred_run is true, then this is a deferred run; this affects how
/// certain buffering works.
/// An error return here indicates that the process failed to launch, and the rest of
/// the pipeline should be cancelled.
static launch_result_t exec_process_in_job(parser_t &parser, process_t *p,
                                           const std::shared_ptr<job_t> &j,
                                           const io_chain_t &block_io, autoclose_pipes_t pipes,
                                           const autoclose_pipes_t &deferred_pipes,
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
    trace_if_enabled(parser, L"", p->argv());

    // The IO chain for this process.
    io_chain_t process_net_io_chain = block_io;

    if (pipes.write.valid()) {
        process_net_io_chain.push_back(std::make_shared<io_pipe_t>(
            p->pipe_write_fd, false /* not input */, std::move(pipes.write)));
    }

    // Append IOs from the process's redirection specs.
    // This may fail, e.g. a failed redirection.
    if (!process_net_io_chain.append_from_specs(p->redirection_specs(),
                                                parser.vars().get_pwd_slash())) {
        return launch_result_t::failed;
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

    // Decide if outputting to a pipe may deadlock.
    // This happens if fish pipes from an internal process into another internal process:
    //    echo $big | string match...
    // Here fish will only run one process at a time, so the pipe buffer may overfill.
    // It may also happen when piping internal -> external:
    //    echo $big | external_proc
    // fish wants to run `echo` before launching external_proc, so the pipe may deadlock.
    // However if we are a deferred run, it means that we are piping into an external process
    // which got launched before us!
    bool piped_output_needs_buffering = !p->is_last_in_job && !is_deferred_run;

    // Execute the process.
    p->check_generations_before_launch();
    switch (p->type) {
        case process_type_t::function:
        case process_type_t::block_node: {
            if (exec_block_or_func_process(parser, j, p, process_net_io_chain,
                                           piped_output_needs_buffering) ==
                launch_result_t::failed) {
                return launch_result_t::failed;
            }
            break;
        }

        case process_type_t::builtin: {
            if (exec_builtin_process(parser, j, p, process_net_io_chain,
                                     piped_output_needs_buffering) == launch_result_t::failed) {
                return launch_result_t::failed;
            }
            break;
        }

        case process_type_t::external: {
            if (exec_external_command(parser, j, p, process_net_io_chain) ==
                launch_result_t::failed) {
                return launch_result_t::failed;
            }
            // It's possible (though unlikely) that this is a background process which recycled a
            // pid from another, previous background process. Forget any such old process.
            parser.get_wait_handles_ffi()->remove_by_pid(p->pid);
            break;
        }

        case process_type_t::exec: {
            // We should have handled exec up above.
            DIE("process_type_t::exec process found in pipeline, where it should never be. "
                "Aborting.");
        }
    }
    return launch_result_t::ok;
}

// Do we have a fish internal process that pipes into a real process? If so, we are going to
// launch it last (if there's more than one, just the last one). That is to prevent buffering
// from blocking further processes. See #1396.
// Example:
//   for i in (seq 1 5); sleep 1; echo $i; end | cat
// This should show the output as it comes, not buffer until the end.
// Any such process (only one per job) will be called the "deferred" process.
static process_t *get_deferred_process(const shared_ptr<job_t> &j) {
    // Common case is no deferred proc.
    if (j->processes.size() <= 1) return nullptr;

    // Skip execs, which can only appear at the front.
    if (j->processes.front()->type == process_type_t::exec) return nullptr;

    // Find the last non-external process, and return it if it pipes into an extenal process.
    for (auto i = j->processes.rbegin(); i != j->processes.rend(); ++i) {
        process_t *p = i->get();
        if (p->type != process_type_t::external) {
            return p->is_last_in_job ? nullptr : p;
        }
    }
    return nullptr;
}

/// Given that we failed to execute process \p failed_proc in job \p job, mark that process and
/// every subsequent process in the pipeline as aborted before launch.
static void abort_pipeline_from(const shared_ptr<job_t> &job, const process_t *failed_proc) {
    bool found = false;
    for (process_ptr_t &p : job->processes) {
        found = found || (p.get() == failed_proc);
        if (found) p->mark_aborted_before_launch();
    }
    assert(found && "Process not present in job");
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

bool exec_job(parser_t &parser, const shared_ptr<job_t> &j, const io_chain_t &block_io) {
    assert(j && "null job_t passed to exec_job!");

    // If fish was invoked with -n or --no-execute, then no_exec will be set and we do nothing.
    if (no_exec()) {
        return true;
    }

    // Handle an exec call.
    if (j->processes.front()->type == process_type_t::exec) {
        // If we are interactive, perhaps disallow exec if there are background jobs.
        if (!allow_exec_with_background_jobs(parser)) {
            for (const auto &p : j->processes) {
                p->mark_aborted_before_launch();
            }
            return false;
        }

        internal_exec(parser.vars(), j.get(), block_io);
        // internal_exec only returns if it failed to set up redirections.
        // In case of an successful exec, this code is not reached.
        int status = j->flags().negate ? 0 : 1;
        parser.set_last_statuses(statuses_t::just(status));

        // A false return tells the caller to remove the job from the list.
        for (const auto &p : j->processes) {
            p->mark_aborted_before_launch();
        }
        return false;
    }
    auto timer = push_timer(j->wants_timing() && !no_exec());

    // Get the deferred process, if any. We will have to remember its pipes.
    autoclose_pipes_t deferred_pipes;
    process_t *const deferred_process = get_deferred_process(j);

    // We may want to transfer tty ownership to the pgroup leader.
    tty_transfer_t transfer{};

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
    // Lastly, a process may experience a pipeline-aborting error, which prevents launching
    // further processes in the pipeline.
    autoclose_fd_t pipe_next_read;
    bool aborted_pipeline = false;
    size_t procs_launched = 0;
    for (const auto &procptr : j->processes) {
        process_t *p = procptr.get();

        // proc_pipes is the pipes applied to this process. That is, it is the read end
        // containing the output of the previous process (if any), plus the write end that will
        // output to the next process (if any).
        autoclose_pipes_t proc_pipes;
        proc_pipes.read = std::move(pipe_next_read);
        if (!p->is_last_in_job) {
            auto pipes = make_autoclose_pipes();
            if (!pipes) {
                FLOGF(warning, PIPE_ERROR);
                wperror(L"pipe");
                aborted_pipeline = true;
                abort_pipeline_from(j, p);
                break;
            }
            pipe_next_read = std::move(pipes->read);
            proc_pipes.write = std::move(pipes->write);

            // Save any deferred process for last. By definition, the deferred process can never be
            // the last process in the job, so it's safe to nest this in the outer
            // `if (!p->is_last_in_job)` block, which makes it clear that `proc_next_read` will
            // always be assigned when we `continue` the loop.
            if (p == deferred_process) {
                deferred_pipes = std::move(proc_pipes);
                continue;
            }
        }

        // Regular process.
        if (exec_process_in_job(parser, p, j, block_io, std::move(proc_pipes), deferred_pipes) ==
            launch_result_t::failed) {
            aborted_pipeline = true;
            abort_pipeline_from(j, p);
            break;
        }
        procs_launched += 1;

        // Transfer tty?
        if (p->leads_pgrp && j->group->wants_terminal()) {
            transfer.to_job_group(j->group);
        }
    }
    pipe_next_read.close();

    // If our pipeline was aborted before any process was successfully launched, then there is
    // nothing to reap, and we can perform an early return.
    // Note we must never return false if we have launched even one process, since it will not be
    // properly reaped; see #7038.
    if (aborted_pipeline && procs_launched == 0) {
        return false;
    }

    // Ok, at least one thing got launched.
    // Handle any deferred process.
    if (deferred_process) {
        if (aborted_pipeline) {
            // Some other process already aborted our pipeline.
            deferred_process->mark_aborted_before_launch();
        } else if (exec_process_in_job(parser, deferred_process, j, block_io,
                                       std::move(deferred_pipes), {},
                                       true) == launch_result_t::failed) {
            // The deferred proc itself failed to launch.
            deferred_process->mark_aborted_before_launch();
        }
    }

    FLOGF(exec_job_exec, L"Executed job %d from command '%ls'", j->job_id(), j->command_wcstr());

    j->mark_constructed();

    // If exec_error then a backgrounded job would have been terminated before it was ever assigned
    // a pgroup, so error out before setting last_pid.
    if (!j->is_foreground()) {
        maybe_t<pid_t> last_pid = j->get_last_pid();
        if (last_pid.has_value()) {
            parser.vars().set_one(L"last_pid", ENV_GLOBAL, to_string(*last_pid));
        } else {
            parser.vars().set_empty(L"last_pid", ENV_GLOBAL);
        }
    }

    if (!j->is_initially_background()) {
        j->continue_job(parser);
    }

    if (j->is_stopped()) transfer.save_tty_modes();
    transfer.reclaim();
    return true;
}

/// Populate \p lst with the output of \p buffer, perhaps splitting lines according to \p split.
static void populate_subshell_output(std::vector<wcstring> *lst, const separated_buffer_t &buffer,
                                     bool split) {
    // Walk over all the elements.
    for (const auto &elem : buffer.elements()) {
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
/// If \p job_group is set, any spawned commands should join that job group.
/// If \p apply_exit_status is false, then reset $status back to its original value.
/// \p is_subcmd controls whether we apply a read limit.
/// \p break_expand is used to propagate whether the result should be "expansion breaking" in the
/// sense that subshells used during string expansion should halt that expansion. \return the value
/// of $status.
static int exec_subshell_internal(const wcstring &cmd, parser_t &parser,
                                  const job_group_ref_t &job_group, std::vector<wcstring> *lst,
                                  bool *break_expand, bool apply_exit_status, bool is_subcmd) {
    parser.assert_can_execute();
    auto &ld = parser.libdata();

    scoped_push<bool> is_subshell(&ld.is_subshell, true);
    scoped_push<size_t> read_limit(&ld.read_limit, is_subcmd ? read_byte_limit : 0);

    auto prev_statuses = parser.get_last_statuses();
    const cleanup_t put_back([&] {
        if (!apply_exit_status) {
            parser.set_last_statuses(prev_statuses);
        }
    });

    const bool split_output = parser.vars().get_unless_empty(L"IFS").has_value();

    // IO buffer creation may fail (e.g. if we have too many open files to make a pipe), so this may
    // be null.
    auto bufferfill = io_bufferfill_t::create(ld.read_limit);
    if (!bufferfill) {
        *break_expand = true;
        return STATUS_CMD_ERROR;
    }
    eval_res_t eval_res =
        parser.eval_with(cmd, io_chain_t{bufferfill}, job_group, block_type_t::subst);
    separated_buffer_t buffer = io_bufferfill_t::finish(std::move(bufferfill));
    if (buffer.discarded()) {
        *break_expand = true;
        return STATUS_READ_TOO_MUCH;
    }

    if (eval_res.break_expand) {
        *break_expand = true;
        return eval_res.status.status_value();
    }

    if (lst) {
        populate_subshell_output(lst, buffer, split_output);
    }
    *break_expand = false;
    return eval_res.status.status_value();
}

int exec_subshell_for_expand(const wcstring &cmd, parser_t &parser,
                             const job_group_ref_t &job_group, std::vector<wcstring> &outputs) {
    parser.assert_can_execute();
    bool break_expand = false;
    int ret = exec_subshell_internal(cmd, parser, job_group, &outputs, &break_expand, true, true);
    // Only return an error code if we should break expansion.
    return break_expand ? ret : STATUS_CMD_OK;
}

int exec_subshell(const wcstring &cmd, parser_t &parser, bool apply_exit_status) {
    bool break_expand = false;
    return exec_subshell_internal(cmd, parser, nullptr, nullptr, &break_expand, apply_exit_status,
                                  false);
}

int exec_subshell(const wcstring &cmd, parser_t &parser, std::vector<wcstring> &outputs,
                  bool apply_exit_status) {
    bool break_expand = false;
    return exec_subshell_internal(cmd, parser, nullptr, &outputs, &break_expand, apply_exit_status,
                                  false);
}
#endif
