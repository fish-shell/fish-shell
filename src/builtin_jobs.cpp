// Functions for executing the jobs builtin.
#include "config.h"  // IWYU pragma: keep

#include <sys/time.h>

#include <cerrno>
#include <cstddef>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

/// Print modes for the jobs builtin.
enum {
    JOBS_DEFAULT,        // print lots of general info
    JOBS_PRINT_PID,      // print pid of each process in job
    JOBS_PRINT_COMMAND,  // print command name of each process in job
    JOBS_PRINT_GROUP,    // print group id of job
    JOBS_PRINT_NOTHING,  // print nothing (exit status only)
};

/// Calculates the cpu usage (in percent) of the specified job.
static int cpu_use(const job_t *j) {
    double u = 0;

    for (const process_ptr_t &p : j->processes) {
        struct timeval t;
        unsigned long jiffies;
        gettimeofday(&t, nullptr);
        jiffies = proc_get_jiffies(p.get());

        double t1 = 1000000.0 * p->last_time.tv_sec + p->last_time.tv_usec;
        double t2 = 1000000.0 * t.tv_sec + t.tv_usec;

        // Check for a race condition that can cause negative CPU usage to be reported (#7066)
        unsigned long cached_last_jiffies = p->last_jiffies;
        if (t2 < t1 || jiffies < cached_last_jiffies) {
            continue;
        }

        // std::fwprintf( stderr, L"t1 %f t2 %f p1 %d p2 %d\n", t1, t2, jiffies, p->last_jiffies );
        u += (static_cast<double>(jiffies - cached_last_jiffies)) / (t2 - t1);
    }
    return u * 1000000;
}

/// Print information about the specified job.
static void builtin_jobs_print(const job_t *j, int mode, int header, io_streams_t &streams) {
    int pgid = INVALID_PID;
    if (auto job_pgid = j->get_pgid()) {
        pgid = *job_pgid;
    }

    switch (mode) {
        case JOBS_PRINT_NOTHING: {
            break;
        }
        case JOBS_DEFAULT: {
            if (header) {
                // Print table header before first job.
                streams.out.append(_(L"Job\tGroup\t"));
                if (have_proc_stat()) {
                    streams.out.append(_(L"CPU\t"));
                }
                streams.out.append(_(L"State\tCommand\n"));
            }

            streams.out.append_format(L"%d\t%d\t", j->job_id(), pgid);

            if (have_proc_stat()) {
                streams.out.append_format(L"%d%%\t", cpu_use(j));
            }

            streams.out.append(j->is_stopped() ? _(L"stopped") : _(L"running"));
            streams.out.append(L"\t");
            streams.out.append(j->command_wcstr());
            streams.out.append(L"\n");
            break;
        }
        case JOBS_PRINT_GROUP: {
            if (header) {
                // Print table header before first job.
                streams.out.append(_(L"Group\n"));
            }
            streams.out.append_format(L"%d\n", pgid);
            break;
        }
        case JOBS_PRINT_PID: {
            if (header) {
                // Print table header before first job.
                streams.out.append(_(L"Process\n"));
            }

            for (const process_ptr_t &p : j->processes) {
                streams.out.append_format(L"%d\n", p->pid);
            }
            break;
        }
        case JOBS_PRINT_COMMAND: {
            if (header) {
                // Print table header before first job.
                streams.out.append(_(L"Command\n"));
            }

            for (const process_ptr_t &p : j->processes) {
                streams.out.append_format(L"%ls\n", p->argv0());
            }
            break;
        }
        default: {
            DIE("unexpected mode");
        }
    }
}

/// The jobs builtin. Used for printing running jobs. Defined in builtin_jobs.c.
maybe_t<int> builtin_jobs(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool found = false;
    int mode = JOBS_DEFAULT;
    bool print_last = false;

    static const wchar_t *const short_options = L":cghlpq";
    static const struct woption long_options[] = {{L"command", no_argument, nullptr, 'c'},
                                                  {L"group", no_argument, nullptr, 'g'},
                                                  {L"help", no_argument, nullptr, 'h'},
                                                  {L"last", no_argument, nullptr, 'l'},
                                                  {L"pid", no_argument, nullptr, 'p'},
                                                  {L"quiet", no_argument, nullptr, 'q'},
                                                  {L"query", no_argument, nullptr, 'q'},
                                                  {nullptr, 0, nullptr, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'p': {
                mode = JOBS_PRINT_PID;
                break;
            }
            case 'q': {
                mode = JOBS_PRINT_NOTHING;
                break;
            }
            case 'c': {
                mode = JOBS_PRINT_COMMAND;
                break;
            }
            case 'g': {
                mode = JOBS_PRINT_GROUP;
                break;
            }
            case 'l': {
                print_last = true;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
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

    if (print_last) {
        // Ignore unconstructed jobs, i.e. ourself.
        for (const auto &j : parser.jobs()) {
            if (j->is_visible()) {
                builtin_jobs_print(j.get(), mode, !streams.out_is_redirected, streams);
                return STATUS_CMD_OK;
            }
        }
        return STATUS_CMD_ERROR;

    } else {
        if (w.woptind < argc) {
            int i;

            for (i = w.woptind; i < argc; i++) {
                const job_t *j = nullptr;

                if (argv[i][0] == L'%') {
                    int job_id = fish_wcstoi(argv[i] + 1);
                    if (errno || job_id < -1) {
                        streams.err.append_format(_(L"%ls: '%ls' is not a valid job id"), cmd,
                                                  argv[i]);
                        return STATUS_INVALID_ARGS;
                    }
                    j = parser.job_get(job_id);
                } else {
                    int pid = fish_wcstoi(argv[i]);
                    if (errno || pid < 0) {
                        streams.err.append_format(_(L"%ls: '%ls' is not a valid process id\n"), cmd,
                                                  argv[i]);
                        return STATUS_INVALID_ARGS;
                    }
                    j = parser.job_get_from_pid(pid);
                }

                if (j && !j->is_completed() && j->is_constructed()) {
                    builtin_jobs_print(j, mode, false, streams);
                    found = true;
                } else {
                    if (mode != JOBS_PRINT_NOTHING) {
                        streams.err.append_format(_(L"%ls: No suitable job: %ls\n"), cmd, argv[i]);
                    }
                    return STATUS_CMD_ERROR;
                }
            }
        } else {
            for (const auto &j : parser.jobs()) {
                // Ignore unconstructed jobs, i.e. ourself.
                if (j->is_visible()) {
                    builtin_jobs_print(j.get(), mode, !found && !streams.out_is_redirected,
                                       streams);
                    found = true;
                }
            }
        }
    }

    if (!found) {
        // Do not babble if not interactive.
        if (!streams.out_is_redirected && mode != JOBS_PRINT_NOTHING) {
            streams.out.append_format(_(L"%ls: There are no jobs\n"), argv[0]);
        }
        return STATUS_CMD_ERROR;
    }

    return STATUS_CMD_OK;
}
