// Implementation of the fg builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_fg.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "builtin.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "job_group.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

/// Builtin for putting a job in the foreground.
maybe_t<int> builtin_fg(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
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

    job_t *job = nullptr;
    if (optind == argc) {
        // Select last constructed job (i.e. first job in the job queue) that can be brought
        // to the foreground.

        for (const auto &j : parser.jobs()) {
            if (j->is_constructed() && (!j->is_completed()) &&
                ((j->is_stopped() || (!j->is_foreground())) && j->wants_job_control())) {
                job = j.get();
                break;
            }
        }
        if (!job) {
            streams.err.append_format(_(L"%ls: There are no suitable jobs\n"), cmd);
        }
    } else if (optind + 1 < argc) {
        // Specifying more than one job to put to the foreground is a syntax error, we still
        // try to locate the job $argv[1], since we need to determine which error message to
        // emit (ambigous job specification vs malformed job id).
        bool found_job = false;
        int pid = fish_wcstoi(argv[optind]);
        if (errno == 0 && pid > 0) {
            found_job = (parser.job_get_from_pid(pid) != nullptr);
        }

        if (found_job) {
            streams.err.append_format(_(L"%ls: Ambiguous job\n"), cmd);
        } else {
            streams.err.append_format(_(L"%ls: '%ls' is not a job\n"), cmd, argv[optind]);
        }

        job = nullptr;
        builtin_print_error_trailer(parser, streams.err, cmd);
    } else {
        int pid = abs(fish_wcstoi(argv[optind]));
        if (errno) {
            streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, cmd, argv[optind]);
            builtin_print_error_trailer(parser, streams.err, cmd);
        } else {
            job = parser.job_get_from_pid(pid);
            if (!job || !job->is_constructed() || job->is_completed()) {
                streams.err.append_format(_(L"%ls: No suitable job: %d\n"), cmd, pid);
                job = nullptr;
            } else if (!job->wants_job_control()) {
                streams.err.append_format(_(L"%ls: Can't put job %d, '%ls' to foreground because it is not under job control\n"),
                                          cmd, pid, job->command_wcstr());
                job = nullptr;
            }
        }
    }

    if (!job) {
        return STATUS_INVALID_ARGS;
    }

    if (streams.err_is_redirected) {
        streams.err.append_format(FG_MSG, job->job_id(), job->command_wcstr());
    } else {
        // If we aren't redirecting, send output to real stderr, since stuff in sb_err won't get
        // printed until the command finishes.
        std::fwprintf(stderr, FG_MSG, job->job_id(), job->command_wcstr());
    }

    wcstring ft = tok_command(job->command());
    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    if (!ft.empty()) parser.set_var_and_fire(L"_", ENV_EXPORT, std::move(ft));
    reader_write_title(job->command(), parser);

    parser.job_promote(job);
    job->group->set_is_foreground(true);

    job->continue_job(parser);
    return STATUS_CMD_OK;
}
