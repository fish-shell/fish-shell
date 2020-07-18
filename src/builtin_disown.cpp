// Implementation of the disown builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_disown.h"

#include <cerrno>
#include <csignal>
#include <set>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wutil.h"  // IWYU pragma: keep

/// Helper for builtin_disown.
static int disown_job(const wchar_t *cmd, parser_t &parser, io_streams_t &streams, job_t *j) {
    if (j == nullptr) {
        streams.err.append_format(_(L"%ls: Unknown job '%ls'\n"), L"bg");
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    // Stopped disowned jobs must be manually signaled; explain how to do so.
    auto pgid = j->get_pgid();
    if (j->is_stopped()) {
        if (pgid) killpg(*pgid, SIGCONT);
        const wchar_t *fmt =
            _(L"%ls: job %d ('%ls') was stopped and has been signalled to continue.\n");
        streams.err.append_format(fmt, cmd, j->job_id(), j->command_wcstr());
    }

    // We cannot directly remove the job from the jobs() list as `disown` might be called
    // within the context of a subjob which will cause the parent job to crash in exec_job().
    // Instead, we set a flag and the parser removes the job from the jobs list later.
    j->mut_flags().disown_requested = true;
    add_disowned_job(j);

    return STATUS_CMD_OK;
}

/// Builtin for removing jobs from the job list.
maybe_t<int> builtin_disown(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
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

    if (argv[1] == nullptr) {
        // Select last constructed job (ie first job in the job queue) that is possible to disown.
        // Stopped jobs can be disowned (they will be continued).
        // Foreground jobs can be disowned.
        // Even jobs that aren't under job control can be disowned!
        job_t *job = nullptr;
        for (const auto &j : parser.jobs()) {
            if (j->is_constructed() && (!j->is_completed())) {
                job = j.get();
                break;
            }
        }

        if (job) {
            retval = disown_job(cmd, parser, streams, job);
        } else {
            streams.err.append_format(_(L"%ls: There are no suitable jobs\n"), cmd);
            retval = STATUS_CMD_ERROR;
        }
    } else {
        std::set<job_t *> jobs;

        // If one argument is not a valid pid (i.e. integer >= 0), fail without disowning anything,
        // but still print errors for all of them.
        // Non-existent jobs aren't an error, but information about them is useful.
        // Multiple PIDs may refer to the same job; include the job only once by using a set.
        for (int i = 1; argv[i]; i++) {
            int pid = fish_wcstoi(argv[i]);
            if (errno || pid < 0) {
                streams.err.append_format(_(L"%ls: '%ls' is not a valid job specifier\n"), cmd,
                                          argv[i]);
                retval = STATUS_INVALID_ARGS;
            } else {
                if (job_t *j = parser.job_get_from_pid(pid)) {
                    jobs.insert(j);
                } else {
                    streams.err.append_format(_(L"%ls: Could not find job '%d'\n"), cmd, pid);
                }
            }
        }
        if (retval != STATUS_CMD_OK) {
            return retval;
        }

        // Disown all target jobs
        for (const auto &j : jobs) {
            retval |= disown_job(cmd, parser, streams, j);
        }
    }

    return retval;
}
