// Implementation of the bg builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_bg.h"

#include <cerrno>
#include <cstdlib>
#include <memory>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wutil.h"  // IWYU pragma: keep

/// Helper function for builtin_bg().
static int send_to_bg(parser_t &parser, io_streams_t &streams, job_t *j) {
    assert(j != NULL);
    if (!j->wants_job_control()) {
        wcstring error_message = format_string(
            _(L"%ls: Can't put job %d, '%ls' to background because it is not under job control\n"),
            L"bg", j->job_id, j->command_wcstr());
        builtin_print_help(parser, streams, L"bg", &error_message);
        return STATUS_CMD_ERROR;
    }

    streams.err.append_format(_(L"Send job %d '%ls' to background\n"), j->job_id,
                              j->command_wcstr());
    parser.job_promote(j);
    j->mut_flags().foreground = false;
    j->continue_job(parser, true, j->is_stopped());
    return STATUS_CMD_OK;
}

/// Builtin for putting a job in the background.
int builtin_bg(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if (optind == argc) {
        // No jobs were specified so use the most recent (i.e., last) job.
        job_t *job = nullptr;
        for (const auto &j : parser.jobs()) {
            if (j->is_stopped() && j->wants_job_control() && (!j->is_completed())) {
                job = j.get();
                break;
            }
        }

        if (!job) {
            streams.err.append_format(_(L"%ls: There are no suitable jobs\n"), cmd);
            retval = STATUS_CMD_ERROR;
        } else {
            retval = send_to_bg(parser, streams, job);
        }

        return retval;
    }

    // The user specified at least one job to be backgrounded.
    std::vector<int> pids;

    // If one argument is not a valid pid (i.e. integer >= 0), fail without backgrounding anything,
    // but still print errors for all of them.
    for (int i = optind; argv[i]; i++) {
        int pid = fish_wcstoi(argv[i]);
        if (errno || pid < 0) {
            streams.err.append_format(_(L"%ls: '%ls' is not a valid job specifier\n"), L"bg",
                                      argv[i]);
            retval = STATUS_INVALID_ARGS;
        }
        pids.push_back(pid);
    }

    if (retval != STATUS_CMD_OK) return retval;

    // Background all existing jobs that match the pids.
    // Non-existent jobs aren't an error, but information about them is useful.
    for (auto p : pids) {
        if (job_t *j = job_t::from_pid(p)) {
            retval |= send_to_bg(parser, streams, j);
        } else {
            streams.err.append_format(_(L"%ls: Could not find job '%d'\n"), cmd, p);
        }
    }

    return retval;
}
