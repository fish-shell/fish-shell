// Functions for waiting for processes completed.
#include "builtin_wait.h"

#include <sys/wait.h>

#include <algorithm>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "parser.h"
#include "proc.h"
#include "signal.h"
#include "wgetopt.h"
#include "wutil.h"

/// Return the job id to which the process with pid belongs.
/// If a specified process has already finished but the job hasn't, parser_t::job_get_from_pid()
/// doesn't work properly, so use this function in wait command.
static job_id_t get_job_id_from_pid(pid_t pid, const parser_t &parser) {
    for (const auto &j : parser.jobs()) {
        if (j->pgid == pid) {
            return j->job_id();
        }
        // Check if the specified pid is a child process of the job.
        for (const process_ptr_t &p : j->processes) {
            if (p->pid == pid) {
                return j->job_id();
            }
        }
    }
    return 0;
}

static bool all_jobs_finished(const parser_t &parser) {
    for (const auto &j : parser.jobs()) {
        // If any job is not completed, return false.
        // If there are stopped jobs, they are ignored.
        if (j->is_constructed() && !j->is_completed() && !j->is_stopped()) {
            return false;
        }
    }
    return true;
}

static bool any_jobs_finished(size_t jobs_len, const parser_t &parser) {
    bool no_jobs_running = true;

    // If any job is removed from list, return true.
    if (jobs_len != parser.jobs().size()) {
        return true;
    }
    for (const auto &j : parser.jobs()) {
        // If any job is completed, return true.
        if (j->is_constructed() && (j->is_completed() || j->is_stopped())) {
            return true;
        }
        // Check for jobs running exist or not.
        if (j->is_constructed() && !j->is_stopped()) {
            no_jobs_running = false;
        }
    }
    return no_jobs_running;
}

static int wait_for_backgrounds(parser_t &parser, bool any_flag) {
    sigint_checker_t sigint;
    size_t jobs_len = parser.jobs().size();
    while ((!any_flag && !all_jobs_finished(parser)) ||
           (any_flag && !any_jobs_finished(jobs_len, parser))) {
        if (sigint.check()) {
            return 128 + SIGINT;
        }
        proc_wait_any(parser);
    }
    return 0;
}

static bool all_specified_jobs_finished(const parser_t &parser, const std::vector<job_id_t> &ids) {
    for (auto id : ids) {
        if (const job_t *j = parser.job_get(id)) {
            // If any specified job is not completed, return false.
            // If there are stopped jobs, they are ignored.
            if (j->is_constructed() && !j->is_completed() && !j->is_stopped()) {
                return false;
            }
        }
    }
    return true;
}

static bool any_specified_jobs_finished(const parser_t &parser, const std::vector<job_id_t> &ids) {
    for (auto id : ids) {
        if (const job_t *j = parser.job_get(id)) {
            // If any specified job is completed, return true.
            if (j->is_constructed() && (j->is_completed() || j->is_stopped())) {
                return true;
            }
        } else {
            // If any specified job is removed from list, return true.
            return true;
        }
    }
    return false;
}

static int wait_for_backgrounds_specified(parser_t &parser, const std::vector<job_id_t> &ids,
                                          bool any_flag) {
    sigint_checker_t sigint;
    while ((!any_flag && !all_specified_jobs_finished(parser, ids)) ||
           (any_flag && !any_specified_jobs_finished(parser, ids))) {
        if (sigint.check()) {
            return 128 + SIGINT;
        }
        proc_wait_any(parser);
    }
    return 0;
}

/// Tests if all characters in the wide string are numeric.
static bool iswnumeric(const wchar_t *n) {
    for (; *n; n++) {
        if (*n < L'0' || *n > L'9') {
            return false;
        }
    }
    return true;
}

/// See if the process described by \c proc matches the commandline \c cmd.
static bool match_pid(const wcstring &cmd, const wchar_t *proc) {
    // Don't wait for itself
    if (std::wcscmp(proc, L"wait") == 0) return false;

    // Get the command to match against. We're only interested in the last path component.
    const wcstring base_cmd = wbasename(cmd);
    return std::wcscmp(proc, base_cmd.c_str()) == 0;
}

/// It should search the job list for something matching the given proc.
static bool find_job_by_name(const wchar_t *proc, std::vector<job_id_t> &ids,
                             const parser_t &parser) {
    bool found = false;

    for (const auto &j : parser.jobs()) {
        if (j->command().empty()) continue;

        if (match_pid(j->command(), proc)) {
            if (!contains(ids, j->job_id())) {
                // If pids doesn't already have the pgid, add it.
                ids.push_back(j->job_id());
            }
            found = true;
        }

        // Check if the specified pid is a child process of the job.
        for (const process_ptr_t &p : j->processes) {
            if (p->actual_cmd.empty()) continue;

            if (match_pid(p->actual_cmd, proc)) {
                if (!contains(ids, j->job_id())) {
                    // If pids doesn't already have the pgid, add it.
                    ids.push_back(j->job_id());
                }
                found = true;
            }
        }
    }

    return found;
}

/// The following function is invoked on the main thread, because the job operation is not thread
/// safe. It waits for child jobs, not for child processes individually.
int builtin_wait(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    ASSERT_IS_MAIN_THREAD();
    int retval = STATUS_CMD_OK;
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool any_flag = false;  // flag for -n option
    bool print_help = false;

    static const wchar_t *const short_options = L":nh";
    static const struct woption long_options[] = {{L"any", no_argument, nullptr, 'n'},
                                                  {L"help", no_argument, nullptr, 'h'},
                                                  {nullptr, 0, nullptr, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'n':
                any_flag = true;
                break;
            case 'h':
                print_help = true;
                break;
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    if (print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if (w.woptind == argc) {
        // no jobs specified
        retval = wait_for_backgrounds(parser, any_flag);
    } else {
        // jobs specified
        std::vector<job_id_t> waited_job_ids;

        for (int i = w.woptind; i < argc; i++) {
            if (iswnumeric(argv[i])) {
                // argument is pid
                pid_t pid = fish_wcstoi(argv[i]);
                if (errno || pid <= 0) {
                    streams.err.append_format(_(L"%ls: '%ls' is not a valid process id\n"), cmd,
                                              argv[i]);
                    continue;
                }
                if (job_id_t id = get_job_id_from_pid(pid, parser)) {
                    waited_job_ids.push_back(id);
                } else {
                    streams.err.append_format(
                        _(L"%ls: Could not find a job with process id '%d'\n"), cmd, pid);
                }
            } else {
                // argument is process name
                if (!find_job_by_name(argv[i], waited_job_ids, parser)) {
                    streams.err.append_format(
                        _(L"%ls: Could not find child processes with the name '%ls'\n"), cmd,
                        argv[i]);
                }
            }
        }

        if (waited_job_ids.empty()) return STATUS_INVALID_ARGS;

        retval = wait_for_backgrounds_specified(parser, waited_job_ids, any_flag);
    }

    return retval;
}
