// Implementation of the fg builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "builtin.h"
#include "builtin_fg.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "proc.h"
#include "reader.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

/// Builtin for putting a job in the foreground.
int builtin_fg(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    job_t *j = NULL;

    if (optind == argc) {
        // Select last constructed job (I.e. first job in the job que) that is possible to put in
        // the foreground.
        job_iterator_t jobs;
        while ((j = jobs.next())) {
            if (j->is_constructed() && (!j->is_completed()) &&
                ((j->is_stopped() || (!j->is_foreground())) &&
                 j->get_flag(job_flag_t::JOB_CONTROL))) {
                break;
            }
        }
        if (!j) {
            streams.err.append_format(_(L"%ls: There are no suitable jobs\n"), cmd);
        }
    } else if (optind + 1 < argc) {
        // Specifying more than one job to put to the foreground is a syntax error, we still
        // try to locate the job argv[1], since we want to know if this is an ambigous job
        // specification or if this is an malformed job id.
        int pid;
        int found_job = 0;

        pid = fish_wcstoi(argv[optind]);
        if (!(errno || pid < 0)) {
            j = job_t::from_pid(pid);
            if (j) found_job = 1;
        }

        if (found_job) {
            streams.err.append_format(_(L"%ls: Ambiguous job\n"), cmd);
        } else {
            streams.err.append_format(_(L"%ls: '%ls' is not a job\n"), cmd, argv[optind]);
        }

        builtin_print_help(parser, streams, cmd, streams.err);

        j = 0;
    } else {
        int pid = abs(fish_wcstoi(argv[optind]));
        if (errno) {
            streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, cmd, argv[optind]);
            builtin_print_help(parser, streams, cmd, streams.err);
        } else {
            j = job_t::from_pid(pid);
            if (!j || !j->is_constructed() || j->is_completed()) {
                streams.err.append_format(_(L"%ls: No suitable job: %d\n"), cmd, pid);
                j = 0;
            } else if (!j->get_flag(job_flag_t::JOB_CONTROL)) {
                streams.err.append_format(_(L"%ls: Can't put job %d, '%ls' to foreground because "
                                            L"it is not under job control\n"),
                                          cmd, pid, j->command_wcstr());
                j = 0;
            }
        }
    }

    if (!j) {
        return STATUS_INVALID_ARGS;
    }

    if (streams.err_is_redirected) {
        streams.err.append_format(FG_MSG, j->job_id, j->command_wcstr());
    } else {
        // If we aren't redirecting, send output to real stderr, since stuff in sb_err won't get
        // printed until the command finishes.
        fwprintf(stderr, FG_MSG, j->job_id, j->command_wcstr());
    }

    const wcstring ft = tok_first(j->command());
    //For compatibility with fish 2.0's $_, now replaced with `status current-command`
    if (!ft.empty()) env_set_one(L"_", ENV_EXPORT, ft);
    reader_write_title(j->command());

    j->promote();
    j->set_flag(job_flag_t::FOREGROUND, true);

    j->continue_job(j->is_stopped());
    return STATUS_CMD_OK;
}
