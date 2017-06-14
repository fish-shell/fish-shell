// Implementation of the disown builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <signal.h>
#include <stddef.h>

#include <set>

#include "builtin.h"
#include "builtin_disown.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct cmd_opts {
    bool print_help = false;
};

static int parse_cmd_opts(struct cmd_opts *opts, int *optind, int argc, wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    static const wchar_t *short_options = L"h";
    static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                                  {NULL, 0, NULL, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {  //!OCLINT(too few branches)
            case 'h': {
                opts->print_help = true;
                return STATUS_CMD_OK;
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

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// Helper for builtin_disown.
static int disown_job(const wchar_t *cmd, parser_t &parser, io_streams_t &streams, job_t *j) {
    if (j == 0) {
        streams.err.append_format(_(L"%ls: Unknown job '%ls'\n"), L"bg");
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Stopped disowned jobs must be manually signaled; explain how to do so.
    if (job_is_stopped(j)) {
        killpg(j->pgid, SIGCONT);
        const wchar_t *fmt =
            _(L"%ls: job %d ('%ls') was stopped and has been signalled to continue.\n");
        streams.err.append_format(fmt, cmd, j->job_id, j->command_wcstr());
    }

    if (parser.job_remove(j)) return STATUS_CMD_OK;
    return STATUS_CMD_ERROR;
}

/// Builtin for removing jobs from the job list.
int builtin_disown(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    struct cmd_opts opts;

    int optind;
    int retval = parse_cmd_opts(&opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (argv[1] == 0) {
        job_t *j;
        // Select last constructed job (ie first job in the job queue) that is possible to disown.
        // Stopped jobs can be disowned (they will be continued).
        // Foreground jobs can be disowned.
        // Even jobs that aren't under job control can be disowned!
        job_iterator_t jobs;
        while ((j = jobs.next())) {
            if (j->get_flag(JOB_CONSTRUCTED) && (!job_is_completed(j))) {
                break;
            }
        }

        if (j) {
            retval = disown_job(cmd, parser, streams, j);
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
        for (auto j : jobs) {
            retval |= disown_job(cmd, parser, streams, j);
        }
    }

    return retval;
}
