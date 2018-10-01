// Functions for waiting for processes completed.
#include <algorithm>
#include <vector>

#include "builtin.h"
#include "builtin_wait.h"
#include "common.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"

#include <sys/wait.h>

/// Return the job id to which the process with pid belongs.
/// If a specified process has already finished but the job hasn't, parser_t::job_get_from_pid()
/// doesn't work properly, so use this function in wait command.
static job_id_t get_job_id_from_pid(pid_t pid) {
    job_t *j;
    job_iterator_t jobs;

    while ((j = jobs.next()) != nullptr) {
        if (j->pgid == pid) {
            return j->job_id;
        }
        // Check if the specified pid is a child process of the job.
        for (const process_ptr_t &p : j->processes) {
            if (p->pid == pid) {
                return j->job_id;
            }
        }
    }
    return 0;
}

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

static int wait_for_backgrounds(bool any_flag) {
    job_iterator_t jobs;
    size_t jobs_len = jobs.count();

    while ((!any_flag && !all_jobs_finished()) || (any_flag && !any_jobs_finished(jobs_len))) {
        pid_t pid = proc_wait_any();
        if (pid == -1 && errno == EINTR) {
            return 128 + SIGINT;
        }
    }
    return 0;
}

static bool all_specified_jobs_finished(const std::vector<job_id_t> &ids) {
    for (auto id : ids) {
        if (job_t *j = job_get(id)) {
            // If any specified job is not completed, return false.
            // If there are stopped jobs, they are ignored.
            if ((j->flags & JOB_CONSTRUCTED) && !job_is_completed(j) && !job_is_stopped(j)) {
                return false;
            }
        }
    }
    return true;
}

static bool any_specified_jobs_finished(const std::vector<job_id_t> &ids) {
    for (auto id : ids) {
        if (job_t *j = job_get(id)) {
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

static int wait_for_backgrounds_specified(const std::vector<job_id_t> &ids, bool any_flag) {
    while ((!any_flag && !all_specified_jobs_finished(ids)) ||
           (any_flag && !any_specified_jobs_finished(ids))) {
        pid_t pid = proc_wait_any();
        if (pid == -1 && errno == EINTR) {
            return 128 + SIGINT;
        }
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
    if (wcscmp(proc, L"wait") == 0) return false;

    // Get the command to match against. We're only interested in the last path component.
    const wcstring base_cmd = wbasename(cmd);
    return wcscmp(proc, base_cmd.c_str()) == 0;
}

/// It should search the job list for something matching the given proc.
static bool find_job_by_name(const wchar_t *proc, std::vector<job_id_t> &ids) {
    job_iterator_t jobs;
    bool found = false;

    while (const job_t *j = jobs.next()) {
        if (j->command_is_empty()) continue;

        if (match_pid(j->command(), proc)) {
            if (!contains(ids, j->job_id)) {
                // If pids doesn't already have the pgid, add it.
                ids.push_back(j->job_id);
            }
            found = true;
        }

        // Check if the specified pid is a child process of the job.
        for (const process_ptr_t &p : j->processes) {
            if (p->actual_cmd.empty()) continue;

            if (match_pid(p->actual_cmd, proc)) {
                if (!contains(ids, j->job_id)) {
                    // If pids doesn't already have the pgid, add it.
                    ids.push_back(j->job_id);
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
    job_iterator_t jobs;
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool any_flag = false;  // flag for -n option

    static const wchar_t *const short_options = L":n";
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
        retval = wait_for_backgrounds(any_flag);
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
                if (job_id_t id = get_job_id_from_pid(pid)) {
                    waited_job_ids.push_back(id);
                } else {
                    streams.err.append_format(
                        _(L"%ls: Could not find a job with process id '%d'\n"), cmd, pid);
                }
            } else {
                // argument is process name
                if (!find_job_by_name(argv[i], waited_job_ids)) {
                    streams.err.append_format(
                        _(L"%ls: Could not find child processes with the name '%ls'\n"), cmd,
                        argv[i]);
                }
            }
        }

        if (waited_job_ids.empty()) return STATUS_INVALID_ARGS;

        retval = wait_for_backgrounds_specified(waited_job_ids, any_flag);
    }

    return retval;
}
