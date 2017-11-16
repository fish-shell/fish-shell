// Functions for waiting for processes completed.
#include <vector>

#include "builtin.h"
#include "builtin_wait.h"
#include "common.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"

#include <sys/wait.h>

static int retval;

static bool all_jobs_finished() {
    job_iterator_t jobs;
    while (job_t *j = jobs.next()) {
        // If any job is not completed, return false.
        // If there are stopped jobs, they are ignored.
        if ((j->flags & JOB_CONSTRUCTED) && !job_is_completed(j) && !job_is_stopped(j)) {
            return false;
        }
    }
    return true;
}

static bool any_jobs_finished(size_t jobs_len) {
    job_iterator_t jobs;
    bool no_jobs_running = true;

    // If any job is removed from list, return true.
    if (jobs_len != jobs.count()) {
        return true;
    }
    while (job_t *j = jobs.next()) {
        // If any job is completed, return true.
        if ((j->flags & JOB_CONSTRUCTED) && (job_is_completed(j) || job_is_stopped(j))) {
            return true;
        }
        // Check for jobs running exist or not.
        if ((j->flags & JOB_CONSTRUCTED) && !job_is_stopped(j)) {
            no_jobs_running = false;
        }
    }
    if (no_jobs_running) {
        return true;
    }
    return false;
}

static void wait_for_backgrounds(bool any_flag) {
    job_iterator_t jobs;
    size_t jobs_len = jobs.count();

    while ((!any_flag && !all_jobs_finished()) || (any_flag && !any_jobs_finished(jobs_len))) {
        pid_t pid = proc_wait_any();
        if (pid == -1 && errno == EINTR) {
            retval = 128 + SIGINT;
            return;
        }
    }
}

static bool all_specified_jobs_finished(const std::vector<int> &wjobs_pid) {
    for (auto pid : wjobs_pid) {
        if (job_t *j = job_get_from_pid(pid)) {
            // If any specified job is not completed, return false.
            // If there are stopped jobs, they are ignored.
            if ((j->flags & JOB_CONSTRUCTED) && !job_is_completed(j) && !job_is_stopped(j)) {
                return false;
            }
        }
    }
    return true;
}

static bool any_specified_jobs_finished(const std::vector<int> &wjobs_pid) {
    job_t *j;
    for (auto pid : wjobs_pid) {
        if ((j = job_get_from_pid(pid))) {
            // If any specified job is completed, return true.
            if ((j->flags & JOB_CONSTRUCTED) && (job_is_completed(j) || job_is_stopped(j))) {
                return true;
            }
        } else {
            // If any specified job is removed from list, return true.
            return true;
        }
    }
    return false;
}

static void wait_for_backgrounds_specified(const std::vector<int> &wjobs_pid, bool any_flag) {
    while ((!any_flag && !all_specified_jobs_finished(wjobs_pid)) ||
           (any_flag && !any_specified_jobs_finished(wjobs_pid))) {
        pid_t pid = proc_wait_any();
        if (pid == -1 && errno == EINTR) {
            retval = 128 + SIGINT;
            return;
        }
    }
}

int builtin_wait(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    job_t *j;
    job_iterator_t jobs;
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool any_flag = false;  // flag for -n option

    static const wchar_t *short_options = L":n";
    static const struct woption long_options[] = {{L"any", no_argument, NULL, 'n'},
                                                  {NULL, 0, NULL, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                any_flag = true;
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

    if (w.woptind == argc) {
        // no jobs specified
        wait_for_backgrounds(any_flag);
    } else {
        // jobs specified
        std::vector<int> waited_jobs_pid;

        for (int i = w.woptind; i < argc; i++) {
            int pid = fish_wcstoi(argv[i]);
            if (errno || pid < 0) {
                streams.err.append_format(_(L"%ls: '%ls' is not a valid job specifier\n"), cmd,
                                          argv[i]);
                continue;
            }
            if (job_get_from_pid(pid)) {
                waited_jobs_pid.push_back(pid);
            } else {
                // If a specified process has already finished but the job hasn't,
                // job_get_from_pid(pid) doesn't work properly, so check the pgid here.
                while ((j = jobs.next())) {
                    if (j->pgid == pid) {
                        waited_jobs_pid.push_back(pid);
                        break;
                    }
                }
                if (!j) {
                    streams.err.append_format(_(L"%ls: Could not find job '%d'\n"), cmd, pid);
                }
            }
        }

        if (waited_jobs_pid.empty()) return STATUS_INVALID_ARGS;

        wait_for_backgrounds_specified(waited_jobs_pid, any_flag);
    }

    return retval;
}
