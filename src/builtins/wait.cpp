/// Functions for waiting for processes completed.
#include "config.h"  // IWYU pragma: keep

#include "wait.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <deque>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../builtin.h"
#include "../common.h"
#include "../io.h"
#include "../maybe.h"
#include "../parser.h"
#include "../proc.h"
#include "../signal.h"
#include "../topic_monitor.h"
#include "../wait_handle.h"
#include "../wgetopt.h"
#include "../wutil.h"

/// \return true if we can wait on a job.
static bool can_wait_on_job(const std::shared_ptr<job_t> &j) {
    return j->is_constructed() && !j->is_foreground() && !j->is_stopped();
}

/// \return true if a wait handle matches a pid or a process name. Exactly one should be passed.
/// For convenience, this returns false if the wait handle is null.
static bool wait_handle_matches(pid_t pid, const wchar_t *proc_name, const wait_handle_ref_t &wh) {
    assert((pid > 0 || proc_name) && "Must specify either pid or proc_name");
    if (!wh) return false;
    return (pid > 0 && pid == wh->pid) || (proc_name && proc_name == wh->base_name);
}

/// Walk the list of jobs, looking for a process with \p pid (if nonzero) or \p proc_name (if not
/// null). Append all matching wait handles to \p handles.
/// \return true if we found a matching job (even if not waitable), false if not.
static bool find_wait_handles(pid_t pid, const wchar_t *proc_name, const parser_t &parser,
                              std::vector<wait_handle_ref_t> *handles) {
    assert((pid > 0 || proc_name) && "Must specify either pid or proc_name");

    // Has a job already completed?
    // TODO: we can avoid traversing this list if searching by pid.
    bool matched = false;
    for (const auto &wh : parser.get_wait_handles().get_list()) {
        if (wait_handle_matches(pid, proc_name, wh)) {
            handles->push_back(wh);
            matched = true;
        }
    }

    // Is there a running job match?
    for (const auto &j : parser.jobs()) {
        // We want to set 'matched' to true if we could have matched, even if the job was stopped.
        bool provide_handle = can_wait_on_job(j);
        for (const auto &proc : j->processes) {
            auto wh = proc->make_wait_handle(j->internal_job_id);
            if (wait_handle_matches(pid, proc_name, wh)) {
                matched = true;
                if (provide_handle) handles->push_back(std::move(wh));
            }
        }
    }
    return matched;
}

/// \return all wait handles for all jobs, current and already completed (!).
static std::vector<wait_handle_ref_t> get_all_wait_handles(const parser_t &parser) {
    std::vector<wait_handle_ref_t> result;
    // Get wait handles for reaped jobs.
    const auto &whs = parser.get_wait_handles().get_list();
    result.insert(result.end(), whs.begin(), whs.end());

    // Get wait handles for running jobs.
    for (const auto &j : parser.jobs()) {
        if (!can_wait_on_job(j)) continue;
        for (const auto &proc : j->processes) {
            if (auto wh = proc->make_wait_handle(j->internal_job_id)) {
                result.push_back(std::move(wh));
            }
        }
    }
    return result;
}

static inline bool is_completed(const wait_handle_ref_t &wh) { return wh->completed; }

/// Wait for the given wait handles to be marked as completed.
/// If \p any_flag is set, wait for the first one; otherwise wait for all.
/// \return a status code.
static int wait_for_completion(parser_t &parser, const std::vector<wait_handle_ref_t> &whs,
                               bool any_flag) {
    if (whs.empty()) return 0;

    sigchecker_t sigint(topic_t::sighupint);
    for (;;) {
        if (any_flag ? std::any_of(whs.begin(), whs.end(), is_completed)
                     : std::all_of(whs.begin(), whs.end(), is_completed)) {
            // Remove completed wait handles (at most 1 if any_flag is set).
            for (const auto &wh : whs) {
                if (is_completed(wh)) {
                    parser.get_wait_handles().remove(wh);
                    if (any_flag) break;
                }
            }
            return 0;
        }
        if (sigint.check()) {
            return 128 + SIGINT;
        }
        proc_wait_any(parser);
    }
    DIE("Unreachable");
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

maybe_t<int> builtin_wait(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool any_flag = false;  // flag for -n option
    bool print_help = false;

    static const wchar_t *const short_options = L":nh";
    static const struct woption long_options[] = {
        {L"any", no_argument, nullptr, 'n'}, {L"help", no_argument, nullptr, 'h'}, {}};

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
            }
        }
    }

    if (print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if (w.woptind == argc) {
        // No jobs specified.
        // Note this may succeed with an empty wait list.
        return wait_for_completion(parser, get_all_wait_handles(parser), any_flag);
    }

    // Get the list of wait handles for our waiting.
    std::vector<wait_handle_ref_t> wait_handles;
    for (int i = w.woptind; i < argc; i++) {
        if (iswnumeric(argv[i])) {
            // argument is pid
            pid_t pid = fish_wcstoi(argv[i]);
            if (errno || pid <= 0) {
                streams.err.append_format(_(L"%ls: '%ls' is not a valid process id\n"), cmd,
                                          argv[i]);
                continue;
            }
            if (!find_wait_handles(pid, nullptr, parser, &wait_handles)) {
                streams.err.append_format(_(L"%ls: Could not find a job with process id '%d'\n"),
                                          cmd, pid);
            }
        } else {
            // argument is process name
            if (!find_wait_handles(0, argv[i], parser, &wait_handles)) {
                streams.err.append_format(
                    _(L"%ls: Could not find child processes with the name '%ls'\n"), cmd, argv[i]);
            }
        }
    }
    if (wait_handles.empty()) return STATUS_INVALID_ARGS;
    return wait_for_completion(parser, wait_handles, any_flag);
}
