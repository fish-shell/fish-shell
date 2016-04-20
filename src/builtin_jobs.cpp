// Functions for executing the jobs builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdbool.h>
#ifdef HAVE__PROC_SELF_STAT
#include <sys/time.h>
#endif

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
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
};

#ifdef HAVE__PROC_SELF_STAT
/// Calculates the cpu usage (in percent) of the specified job.
static int cpu_use(const job_t *j) {
    double u = 0;
    process_t *p;

    for (p = j->first_process; p; p = p->next) {
        struct timeval t;
        int jiffies;
        gettimeofday(&t, 0);
        jiffies = proc_get_jiffies(p);

        double t1 = 1000000.0 * p->last_time.tv_sec + p->last_time.tv_usec;
        double t2 = 1000000.0 * t.tv_sec + t.tv_usec;

        // fwprintf( stderr, L"t1 %f t2 %f p1 %d p2 %d\n", t1, t2, jiffies, p->last_jiffies );
        u += ((double)(jiffies - p->last_jiffies)) / (t2 - t1);
    }
    return u * 1000000;
}
#endif

/// Print information about the specified job.
static void builtin_jobs_print(const job_t *j, int mode, int header, io_streams_t &streams) {
    process_t *p;
    switch (mode) {
        case JOBS_DEFAULT: {
            if (header) {
                // Print table header before first job.
                streams.out.append(_(L"Job\tGroup\t"));
#ifdef HAVE__PROC_SELF_STAT
                streams.out.append(_(L"CPU\t"));
#endif
                streams.out.append(_(L"State\tCommand\n"));
            }

            streams.out.append_format(L"%d\t%d\t", j->job_id, j->pgid);

#ifdef HAVE__PROC_SELF_STAT
            streams.out.append_format(L"%d%%\t", cpu_use(j));
#endif
            streams.out.append(job_is_stopped(j) ? _(L"stopped") : _(L"running"));
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
            streams.out.append_format(L"%d\n", j->pgid);
            break;
        }
        case JOBS_PRINT_PID: {
            if (header) {
                // Print table header before first job.
                streams.out.append(_(L"Process\n"));
            }

            for (p = j->first_process; p; p = p->next) {
                streams.out.append_format(L"%d\n", p->pid);
            }
            break;
        }
        case JOBS_PRINT_COMMAND: {
            if (header) {
                // Print table header before first job.
                streams.out.append(_(L"Command\n"));
            }

            for (p = j->first_process; p; p = p->next) {
                streams.out.append_format(L"%ls\n", p->argv0());
            }
            break;
        }
    }
}

/// The jobs builtin. Used fopr printing running jobs. Defined in builtin_jobs.c.
int builtin_jobs(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    int argc = 0;
    int found = 0;
    int mode = JOBS_DEFAULT;
    int print_last = 0;

    argc = builtin_count_args(argv);
    w.woptind = 0;

    while (1) {
        static const struct woption long_options[] = {
            {L"pid", no_argument, 0, 'p'},   {L"command", no_argument, 0, 'c'},
            {L"group", no_argument, 0, 'g'}, {L"last", no_argument, 0, 'l'},
            {L"help", no_argument, 0, 'h'},  {0, 0, 0, 0}};

        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"pclgh", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);

                builtin_print_help(parser, streams, argv[0], streams.err);
                return 1;
            }
            case 'p': {
                mode = JOBS_PRINT_PID;
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
                print_last = 1;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return 0;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return 1;
            }
        }
    }

    if (print_last) {
        // Ignore unconstructed jobs, i.e. ourself.
        job_iterator_t jobs;
        const job_t *j;
        while ((j = jobs.next())) {
            if ((j->flags & JOB_CONSTRUCTED) && !job_is_completed(j)) {
                builtin_jobs_print(j, mode, !streams.out_is_redirected, streams);
                return 0;
            }
        }

    } else {
        if (w.woptind < argc) {
            int i;

            for (i = w.woptind; i < argc; i++) {
                int pid;
                wchar_t *end;
                errno = 0;
                pid = fish_wcstoi(argv[i], &end, 10);
                if (errno || *end) {
                    streams.err.append_format(_(L"%ls: '%ls' is not a job\n"), argv[0], argv[i]);
                    return 1;
                }

                const job_t *j = job_get_from_pid(pid);

                if (j && !job_is_completed(j)) {
                    builtin_jobs_print(j, mode, false, streams);
                    found = 1;
                } else {
                    streams.err.append_format(_(L"%ls: No suitable job: %d\n"), argv[0], pid);
                    return 1;
                }
            }
        } else {
            job_iterator_t jobs;
            const job_t *j;
            while ((j = jobs.next())) {
                // Ignore unconstructed jobs, i.e. ourself.
                if ((j->flags & JOB_CONSTRUCTED) && !job_is_completed(j)) {
                    builtin_jobs_print(j, mode, !streams.out_is_redirected, streams);
                    found = 1;
                }
            }
        }
    }

    if (!found) {
        // Do not babble if not interactive.
        if (!streams.out_is_redirected) {
            streams.out.append_format(_(L"%ls: There are no jobs\n"), argv[0]);
        }
        return 1;
    }

    return 0;
}
