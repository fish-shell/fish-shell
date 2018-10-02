// Utilities for keeping track of jobs, processes and subshells, as well as signal handling
// functions for tracking children. These functions do not themselves launch new processes, the exec
// library will call proc to create representations of the running jobs as needed.
//
// Some of the code in this file is based on code from the Glibc manual.
// IWYU pragma: no_include <__bit_reference>
#include "config.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#if HAVE_TERM_H
#include <curses.h>
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <termios.h>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/time.h>  // IWYU pragma: keep
#include <sys/types.h>

#include <algorithm>  // IWYU pragma: keep
#include <memory>
#include <utility>
#include <vector>

#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "output.h"
#include "parse_tree.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "sanity.h"
#include "signal.h"
#include "util.h"
#include "wutil.h"  // IWYU pragma: keep

/// Size of buffer for reading buffered output.
#define BUFFER_SIZE 4096

/// Status of last process to exit.
static int last_status = 0;

/// The signals that signify crashes to us.
static const int crashsignals[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGSYS};

bool job_list_is_empty() {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_list().empty();
}

void job_iterator_t::reset() {
    this->current = job_list->begin();
    this->end = job_list->end();
}

job_iterator_t::job_iterator_t(job_list_t &jobs) : job_list(&jobs) { this->reset(); }

job_iterator_t::job_iterator_t() : job_list(&parser_t::principal_parser().job_list()) {
    ASSERT_IS_MAIN_THREAD();
    this->reset();
}

size_t job_iterator_t::count() const { return this->job_list->size(); }

#if 0
// This isn't used so the lint tools were complaining about its presence. I'm keeping it in the
// source because it could be useful for debugging. However, it would probably be better to add a
// verbose or debug option to the builtin `jobs` command.
void print_jobs(void)
{
    job_iterator_t jobs;
    job_t *j;
    while (j = jobs.next()) {
        fwprintf(stdout, L"%p -> %ls -> (foreground %d, complete %d, stopped %d, constructed %d)\n",
                 j, j->command_wcstr(), j->get_flag(JOB_FOREGROUND), job_is_completed(j),
                 job_is_stopped(j), j->get_flag(JOB_CONSTRUCTED));
    }
}
#endif

bool is_interactive_session = false;
bool is_subshell = false;
bool is_block = false;
bool is_breakpoint = false;
bool is_login = false;
int is_event = false;
pid_t proc_last_bg_pid = 0;
int job_control_mode = JOB_CONTROL_INTERACTIVE;
int no_exec = 0;

static int is_interactive = -1;

static bool proc_had_barrier = false;

bool shell_is_interactive() {
    ASSERT_IS_MAIN_THREAD();
    // is_interactive is statically initialized to -1. Ensure it has been dynamically set
    // before we're called.
    assert(is_interactive != -1);
    return is_interactive > 0;
}

bool get_proc_had_barrier() {
    ASSERT_IS_MAIN_THREAD();
    return proc_had_barrier;
}

void set_proc_had_barrier(bool flag) {
    ASSERT_IS_MAIN_THREAD();
    proc_had_barrier = flag;
}

/// The event variable used to send all process event.
static event_t event(0);

/// A stack containing the values of is_interactive. Used by proc_push_interactive and
/// proc_pop_interactive.
static std::vector<int> interactive_stack;

void proc_init() { proc_push_interactive(0); }

/// Remove job from list of jobs.
static int job_remove(job_t *j) {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_remove(j);
}

void job_promote(job_t *job) {
    ASSERT_IS_MAIN_THREAD();
    parser_t::principal_parser().job_promote(job);
}

void proc_destroy() {
    job_list_t &jobs = parser_t::principal_parser().job_list();
    while (!jobs.empty()) {
        job_t *job = jobs.front().get();
        debug(2, L"freeing leaked job %ls", job->command_wcstr());
        job_remove(job);
    }
}

void proc_set_last_status(int s) {
    ASSERT_IS_MAIN_THREAD();
    last_status = s;
}

int proc_get_last_status() { return last_status; }

// Basic thread safe job IDs. The vector consumed_job_ids has a true value wherever the job ID
// corresponding to that slot is in use. The job ID corresponding to slot 0 is 1.
static owning_lock<std::vector<bool>> locked_consumed_job_ids;

job_id_t acquire_job_id() {
    auto consumed_job_ids = locked_consumed_job_ids.acquire();

    // Find the index of the first 0 slot.
    auto slot = std::find(consumed_job_ids->begin(), consumed_job_ids->end(), false);
    if (slot != consumed_job_ids->end()) {
        // We found a slot. Note that slot 0 corresponds to job ID 1.
        *slot = true;
        return (job_id_t)(slot - consumed_job_ids->begin() + 1);
    }

    // We did not find a slot; create a new slot. The size of the vector is now the job ID
    // (since it is one larger than the slot).
    consumed_job_ids->push_back(true);
    return (job_id_t)consumed_job_ids->size();
}

void release_job_id(job_id_t jid) {
    assert(jid > 0);
    auto consumed_job_ids = locked_consumed_job_ids.acquire();
    size_t slot = (size_t)(jid - 1), count = consumed_job_ids->size();

    // Make sure this slot is within our vector and is currently set to consumed.
    assert(slot < count);
    assert(consumed_job_ids->at(slot) == true);

    // Clear it and then resize the vector to eliminate unused trailing job IDs.
    consumed_job_ids->at(slot) = false;
    while (count--) {
        if (consumed_job_ids->at(count)) break;
    }
    consumed_job_ids->resize(count + 1);
}

job_t *job_get(job_id_t id) {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_get(id);
}

job_t *job_get_from_pid(int pid) {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_get_from_pid(pid);
}

/// Return true if all processes in the job have stopped or completed.
///
/// \param j the job to test
int job_is_stopped(const job_t *j) {
    for (const process_ptr_t &p : j->processes) {
        if (!p->completed && !p->stopped) {
            return 0;
        }
    }
    return 1;
}

/// Return true if the last processes in the job has completed.
///
/// \param j the job to test
bool job_is_completed(const job_t *j) {
    assert(!j->processes.empty());
    bool result = true;
    for (const process_ptr_t &p : j->processes) {
        if (!p->completed) {
            result = false;
            break;
        }
    }
    return result;
}

void job_t::set_flag(job_flag_t flag, bool set) {
    if (set) {
        this->flags |= flag;
    } else {
        this->flags &= ~flag;
    }
}

bool job_t::get_flag(job_flag_t flag) const { return (this->flags & flag) == flag; }

int job_signal(job_t *j, int signal) {
    pid_t my_pgid = getpgrp();
    int res = 0;

    if (j->pgid != my_pgid) {
        res = killpg(j->pgid, signal);
    } else {
        for (const process_ptr_t &p : j->processes) {
            if (!p->completed && p->pid && kill(p->pid, signal)) {
                res = -1;
                break;
            }
        }
    }

    return res;
}

static void mark_job_complete(const job_t *j) {
    for (auto &p : j->processes) {
        p->completed = 1;
    }
}

/// Store the status of the process pid that was returned by waitpid.
static void mark_process_status(process_t *p, int status) {
    // debug( 0, L"Process %ls %ls", p->argv[0], WIFSTOPPED (status)?L"stopped":(WIFEXITED( status
    // )?L"exited":(WIFSIGNALED( status )?L"signaled to exit":L"BLARGH")) );
    p->status = status;

    if (WIFSTOPPED(status)) {
        p->stopped = 1;
    } else if (WIFSIGNALED(status) || WIFEXITED(status)) {
        p->completed = 1;
    } else {
        // This should never be reached.
        p->completed = 1;
        debug(1, "Process %ld exited abnormally", (long)p->pid);
    }
}

void job_mark_process_as_failed(job_t *job, const process_t *failed_proc) {
    // The given process failed to even lift off (e.g. posix_spawn failed) and so doesn't have a
    // valid pid. Mark it and everything after it as dead.
    bool found = false;
    for (process_ptr_t &p : job->processes) {
        found = found || (p.get() == failed_proc);
        if (found) {
            p->completed = true;
        }
    }
}

/// Handle status update for child \c pid.
///
/// \param pid the pid of the process whose status changes
/// \param status the status as returned by wait
static void handle_child_status(pid_t pid, int status) {
    job_t *j = NULL;
    const process_t *found_proc = NULL;

    job_iterator_t jobs;
    while (!found_proc && (j = jobs.next())) {
        process_t *prev = NULL;
        for (process_ptr_t &p : j->processes) {
            if (pid == p->pid) {
                mark_process_status(p.get(), status);
                found_proc = p.get();
                break;
            }
            prev = p.get();
        }
    }

    // If the child process was not killed by a signal or other than SIGINT or SIGQUIT we're done.
    if (!WIFSIGNALED(status) || (WTERMSIG(status) != SIGINT && WTERMSIG(status) != SIGQUIT)) {
        return;
    }

    if (is_interactive_session) {
        // In an interactive session, tell the principal parser to skip all blocks we're executing
        // so control-C returns control to the user.
        if (found_proc) parser_t::skip_all_blocks();
    } else {
        // Deliver the SIGINT or SIGQUIT signal to ourself since we're not interactive.
        struct sigaction act;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = SIG_DFL;
        sigaction(SIGINT, &act, 0);
        sigaction(SIGQUIT, &act, 0);
        kill(getpid(), WTERMSIG(status));
    }
}

process_t::process_t() {}

job_t::job_t(job_id_t jobid, io_chain_t bio)
    : block_io(std::move(bio)), pgid(INVALID_PID), tmodes(), job_id(jobid), flags(0) {}

job_t::~job_t() { release_job_id(job_id); }

/// Return all the IO redirections. Start with the block IO, then walk over the processes.
io_chain_t job_t::all_io_redirections() const {
    io_chain_t result = this->block_io;
    for (const process_ptr_t &p : this->processes) {
        result.append(p->io_chain());
    }
    return result;
}

typedef unsigned int process_generation_count_t;

/// A static value tracking how many SIGCHLDs we have seen. This is only ever modified from within
/// the SIGCHLD signal handler, and therefore does not need atomics or locks.
static volatile process_generation_count_t s_sigchld_generation_cnt = 0;

/// If we have received a SIGCHLD signal, reap exited children of fully-constructed jobs. We cannot
/// reap **all** children (as in, `waitpid(-1, ...)`) since that may reap a pgrp leader that has
/// exited but has another process in its job that has yet to launch and join its pgrp (#5219).
/// If await is false, this returns immediately if no SIGCHLD has been received. If await is true,
/// this waits for one. Returns true if something was processed.
/// This returns the number of children processed, or -1 on error.
static int process_mark_finished_children(bool wants_await) {
    ASSERT_IS_MAIN_THREAD();

    // A static value tracking the SIGCHLD gen count at the time we last processed it. When this is
    // different from s_sigchld_generation_cnt, it indicates there may be unreaped processes.
    // There may not be if we reaped them via the other waitpid path. This is only ever modified
    // from the main thread, and not from a signal handler.
    static process_generation_count_t s_last_sigchld_generation_cnt = 0;

    int processed_count = 0;
    bool got_error = false;

    // The critical read. This fetches a value which is only written in the signal handler. This
    // needs to be an atomic read (we'd use sig_atomic_t, if we knew that were unsigned -
    // fortunately aligned unsigned int is atomic on pretty much any modern chip.) It also needs to
    // occur before we start reaping, since the signal handler can be invoked at any point.
    const process_generation_count_t local_count = s_sigchld_generation_cnt;

    // Determine whether we have children to process. Note that we can't reliably use the difference
    // because a single SIGCHLD may be delivered for multiple children - see #1768. Also if we are
    // awaiting, we always process.
    bool wants_waitpid = wants_await || local_count != s_last_sigchld_generation_cnt;

    if (wants_waitpid) {
        for (;;) {
            // Call waitpid until we get 0/ECHILD. If we wait, it's only on the first iteration. So
            // we want to set NOHANG (don't wait) unless wants_await is true and this is the first
            // iteration.
            int options = WUNTRACED;
            if (!(wants_await && processed_count == 0)) {
                options |= WNOHANG;
            }

            int status = -1;
            pid_t pid = 0;

            bool any_jobs = false;
            // Reap only processes belonging to fully-constructed jobs to prevent reaping of processes
            // before other processes in the same process group have a chance to join their pgrp.
            job_t *j; job_iterator_t jobs;
            while ((j = jobs.next())) {
                any_jobs = true;
                if (j->pgid == INVALID_PID || !j->get_flag(JOB_CONSTRUCTED)) {
                    // Job has not been fully constructed yet
                    debug(4, "Skipping iteration of not fully constructed job %d", j->pgid);
                    continue;
                }

                assert(j->pgid != 0);
                debug(4, "Waiting on processes from job %d", j->pgid);
                pid = waitpid(-1 * j->pgid, &status, options);
                if (pid != 0) {
                    // We'll handle this below
                    break;
                }
            }

            if (!any_jobs) {
                debug(4, "No jobs found to wait for!");
            }

            if (pid > 0) {
                handle_child_status(pid, status);
                processed_count += 1;
                continue;
            }
            else if (pid == 0) {
                // No ready-and-waiting children, we're done.
                break;
            } else {
                // This indicates an error. One likely failure is ECHILD (no children), which we
                // break on, and is not considered an error. The other likely failure is EINTR,
                // which means we got a signal, which is considered an error.
                got_error = (errno != ECHILD);
                break;
            }
        }
    }

    if (got_error) {
        return -1;
    }
    s_last_sigchld_generation_cnt = local_count;
    return processed_count;
}

/// This is called from a signal handler. The signal is always SIGCHLD.
void job_handle_signal(int signal, siginfo_t *info, void *context) {
    UNUSED(signal);
    UNUSED(info);
    UNUSED(context);
    // This is the only place that this generation count is modified. It's OK if it overflows.
    s_sigchld_generation_cnt += 1;
}

/// Given a command like "cat file", truncate it to a reasonable length.
static wcstring truncate_command(const wcstring &cmd) {
    const size_t max_len = 32;
    if (cmd.size() <= max_len) {
        // No truncation necessary.
        return cmd;
    }

    // Truncation required.
    const size_t ellipsis_length = wcslen(ellipsis_str); //no need for wcwidth
    size_t trunc_length = max_len - ellipsis_length;
    // Eat trailing whitespace.
    while (trunc_length > 0 && iswspace(cmd.at(trunc_length - 1))) {
        trunc_length -= 1;
    }
    wcstring result = wcstring(cmd, 0, trunc_length);
    // Append ellipsis.
    result.append(ellipsis_str);
    return result;
}

/// Format information about job status for the user to look at.
typedef enum { JOB_STOPPED, JOB_ENDED } job_status_t;
static void format_job_info(const job_t *j, job_status_t status) {
    const wchar_t *msg = L"Job %d, '%ls' has ended";  // this is the most common status msg
    if (status == JOB_STOPPED) msg = L"Job %d, '%ls' has stopped";

    fwprintf(stdout, L"\r");
    fwprintf(stdout, _(msg), j->job_id, truncate_command(j->command()).c_str());
    fflush(stdout);
    if (cur_term) {
        tputs(clr_eol, 1, &writeb);
    } else {
        fwprintf(stdout, L"\x1B[K");
    }
    fwprintf(stdout, L"\n");
}

void proc_fire_event(const wchar_t *msg, int type, pid_t pid, int status) {
    event.type = type;
    event.param1.pid = pid;

    event.arguments.push_back(msg);
    event.arguments.push_back(to_string<int>(pid));
    event.arguments.push_back(to_string<int>(status));
    event_fire(&event);
    event.arguments.resize(0);
}

static int process_clean_after_marking(bool allow_interactive) {
    ASSERT_IS_MAIN_THREAD();
    job_t *jnext;
    int found = 0;

    // this function may fire an event handler, we do not want to call ourselves recursively (to avoid
    // infinite recursion).
    static bool locked = false;
    if (locked) {
        return 0;
    }
    locked = true;

    // this may be invoked in an exit handler, after the TERM has been torn down
    // don't try to print in that case (#3222)
    const bool interactive = allow_interactive && cur_term != NULL;


    job_iterator_t jobs;
    const size_t job_count = jobs.count();
    jnext = jobs.next();
    while (jnext) {
        job_t *j = jnext;
        jnext = jobs.next();

        // If we are reaping only jobs who do not need status messages sent to the console, do not
        // consider reaping jobs that need status messages.
        if ((!j->get_flag(JOB_SKIP_NOTIFICATION)) && (!interactive) &&
            (!j->get_flag(JOB_FOREGROUND))) {
            continue;
        }

        for (const process_ptr_t &p : j->processes) {
            int s;
            if (!p->completed) continue;

            if (!p->pid) continue;

            s = p->status;

            // TODO: The generic process-exit event is useless and unused.
            // Remove this in future.
            proc_fire_event(L"PROCESS_EXIT", EVENT_EXIT, p->pid,
                            (WIFSIGNALED(s) ? -1 : WEXITSTATUS(s)));

            // Ignore signal SIGPIPE.We issue it ourselves to the pipe writer when the pipe reader
            // dies.
            if (!WIFSIGNALED(s) || WTERMSIG(s) == SIGPIPE) {
                continue;
            }

            // Handle signals other than SIGPIPE.
            int proc_is_job = (p->is_first_in_job && p->is_last_in_job);
            if (proc_is_job) j->set_flag(JOB_NOTIFIED, true);
            // Always report crashes.
            if (j->get_flag(JOB_SKIP_NOTIFICATION) && !contains(crashsignals,WTERMSIG(p->status))) {
                continue;
            }

            // Print nothing if we get SIGINT in the foreground process group, to avoid spamming
            // obvious stuff on the console (#1119). If we get SIGINT for the foreground
            // process, assume the user typed ^C and can see it working. It's possible they
            // didn't, and the signal was delivered via pkill, etc., but the SIGINT/SIGTERM
            // distinction is precisely to allow INT to be from a UI
            // and TERM to be programmatic, so this assumption is keeping with the design of
            // signals. If echoctl is on, then the terminal will have written ^C to the console.
            // If off, it won't have. We don't echo ^C either way, so as to respect the user's
            // preference.
            if (WTERMSIG(p->status) != SIGINT || !j->get_flag(JOB_FOREGROUND)) {
                if (proc_is_job) {
                    // We want to report the job number, unless it's the only job, in which case
                    // we don't need to.
                    const wcstring job_number_desc =
                        (job_count == 1) ? wcstring() : format_string(_(L"Job %d, "), j->job_id);
                    fwprintf(stdout, _(L"%ls: %ls\'%ls\' terminated by signal %ls (%ls)"),
                             program_name, job_number_desc.c_str(),
                             truncate_command(j->command()).c_str(), sig2wcs(WTERMSIG(p->status)),
                             signal_get_desc(WTERMSIG(p->status)));
                } else {
                    const wcstring job_number_desc =
                        (job_count == 1) ? wcstring() : format_string(L"from job %d, ", j->job_id);
                    const wchar_t *fmt =
                        _(L"%ls: Process %d, \'%ls\' %ls\'%ls\' terminated by signal %ls (%ls)");
                    fwprintf(stdout, fmt, program_name, p->pid, p->argv0(), job_number_desc.c_str(),
                             truncate_command(j->command()).c_str(), sig2wcs(WTERMSIG(p->status)),
                             signal_get_desc(WTERMSIG(p->status)));
                }

                if (cur_term != NULL) {
                    tputs(clr_eol, 1, &writeb);
                } else {
                    fwprintf(stdout, L"\x1B[K");  // no term set up - do clr_eol manually
                }
                fwprintf(stdout, L"\n");
            }
            found = 1;
            p->status = 0;  // clear status so it is not reported more than once
        }

        // If all processes have completed, tell the user the job has completed and delete it from
        // the active job list.
        if (job_is_completed(j)) {
            if (!j->get_flag(JOB_FOREGROUND) && !j->get_flag(JOB_NOTIFIED) &&
                !j->get_flag(JOB_SKIP_NOTIFICATION)) {
                format_job_info(j, JOB_ENDED);
                found = 1;
            }
            // TODO: The generic process-exit event is useless and unused.
            // Remove this in future.
            // Don't fire the exit-event for jobs with pgid INVALID_PID.
            // That's our "sentinel" pgid, for jobs that don't (yet) have a pgid,
            // or jobs that consist entirely of builtins (and hence don't have a process).
            // This causes issues if fish is PID 2, which is quite common on WSL. See #4582.
            if (j->pgid != INVALID_PID) {
                proc_fire_event(L"JOB_EXIT", EVENT_EXIT, -j->pgid, 0);
            }
            proc_fire_event(L"JOB_EXIT", EVENT_JOB_ID, j->job_id, 0);

            job_remove(j);
        } else if (job_is_stopped(j) && !j->get_flag(JOB_NOTIFIED)) {
            // Notify the user about newly stopped jobs.
            if (!j->get_flag(JOB_SKIP_NOTIFICATION)) {
                format_job_info(j, JOB_STOPPED);
                found = 1;
            }
            j->set_flag(JOB_NOTIFIED, true);
        }
    }

    if (found) fflush(stdout);

    locked = false;

    return found;
}

int job_reap(bool allow_interactive) {
    ASSERT_IS_MAIN_THREAD();
    int found = 0;

    process_mark_finished_children(false);

    // Preserve the exit status.
    const int saved_status = proc_get_last_status();

    found = process_clean_after_marking(allow_interactive);

    // Restore the exit status.
    proc_set_last_status(saved_status);

    return found;
}

#ifdef HAVE__PROC_SELF_STAT

/// Maximum length of a /proc/[PID]/stat filename.
#define FN_SIZE 256

/// Get the CPU time for the specified process.
unsigned long proc_get_jiffies(process_t *p) {
    if (p->pid <= 0) return 0;

    wchar_t fn[FN_SIZE];
    char state;
    int pid, ppid, pgrp, session, tty_nr, tpgid, exit_signal, processor;
    long int cutime, cstime, priority, nice, placeholder, itrealvalue, rss;
    unsigned long int flags, minflt, cminflt, majflt, cmajflt, utime, stime, starttime, vsize, rlim,
        startcode, endcode, startstack, kstkesp, kstkeip, signal, blocked, sigignore, sigcatch,
        wchan, nswap, cnswap;
    char comm[1024];

    swprintf(fn, FN_SIZE, L"/proc/%d/stat", p->pid);
    FILE *f = wfopen(fn, "r");
    if (!f) return 0;

    // TODO: replace the use of fscanf() as it is brittle and should never be used.
    int count = fscanf(f,
                       "%9d %1023s %c %9d %9d %9d %9d %9d %9lu "
                       "%9lu %9lu %9lu %9lu %9lu %9lu %9ld %9ld %9ld "
                       "%9ld %9ld %9ld %9lu %9lu %9ld %9lu %9lu %9lu "
                       "%9lu %9lu %9lu %9lu %9lu %9lu %9lu %9lu %9lu "
                       "%9lu %9d %9d ",
                       &pid, comm, &state, &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags, &minflt,
                       &cminflt, &majflt, &cmajflt, &utime, &stime, &cutime, &cstime, &priority,
                       &nice, &placeholder, &itrealvalue, &starttime, &vsize, &rss, &rlim,
                       &startcode, &endcode, &startstack, &kstkesp, &kstkeip, &signal, &blocked,
                       &sigignore, &sigcatch, &wchan, &nswap, &cnswap, &exit_signal, &processor);
    fclose(f);
    if (count < 17) return 0;
    return utime + stime + cutime + cstime;
}

/// Update the CPU time for all jobs.
void proc_update_jiffies() {
    job_t *job;
    job_iterator_t j;

    for (job = j.next(); job; job = j.next()) {
        for (process_ptr_t &p : job->processes) {
            gettimeofday(&p->last_time, 0);
            p->last_jiffies = proc_get_jiffies(p.get());
        }
    }
}

#endif

/// Check if there are buffers associated with the job, and select on them for a while if available.
///
/// \param j the job to test
///
/// \return 1 if buffers were available, zero otherwise
static int select_try(job_t *j) {
    fd_set fds;
    int maxfd = -1;

    FD_ZERO(&fds);

    const io_chain_t chain = j->all_io_redirections();
    for (size_t idx = 0; idx < chain.size(); idx++) {
        const io_data_t *io = chain.at(idx).get();
        if (io->io_mode == IO_BUFFER) {
            const io_pipe_t *io_pipe = static_cast<const io_pipe_t *>(io);
            int fd = io_pipe->pipe_fd[0];
            // fwprintf( stderr, L"fd %d on job %ls\n", fd, j->command );
            FD_SET(fd, &fds);
            maxfd = maxi(maxfd, fd);
            debug(3, L"select_try on %d", fd);
        }
    }

    if (maxfd >= 0) {
        int retval;
        struct timeval tv;

        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        retval = select(maxfd + 1, &fds, 0, 0, &tv);
        if (retval == 0) {
            debug(3, L"select_try hit timeout");
        }
        return retval > 0;
    }

    return -1;
}

/// Read from descriptors until they are empty.
///
/// \param j the job to test
static void read_try(job_t *j) {
    io_buffer_t *buff = NULL;

    // Find the last buffer, which is the one we want to read from.
    const io_chain_t chain = j->all_io_redirections();
    for (size_t idx = 0; idx < chain.size(); idx++) {
        io_data_t *d = chain.at(idx).get();
        if (d->io_mode == IO_BUFFER) {
            buff = static_cast<io_buffer_t *>(d);
        }
    }

    if (buff) {
        debug(3, L"proc::read_try('%ls')", j->command_wcstr());
        while (1) {
            char b[BUFFER_SIZE];
            long len = read_blocked(buff->pipe_fd[0], b, BUFFER_SIZE);
            if (len == 0) {
                break;
            } else if (len < 0) {
                if (errno != EAGAIN) {
                    debug(1, _(L"An error occured while reading output from code block"));
                    wperror(L"read_try");
                }
                break;
            } else {
                buff->append(b, len);
            }
        }
    }
}

/// Give ownership of the terminal to the specified job.
///
/// \param j The job to give the terminal to.
/// \param cont If this variable is set, we are giving back control to a job that has previously
/// been stopped. In that case, we need to set the terminal attributes to those saved in the job.
bool terminal_give_to_job(const job_t *j, bool cont) {
    errno = 0;
    if (j->pgid == 0) {
        debug(2, "terminal_give_to_job() returning early due to no process group");
        return true;
    }

    signal_block();

    // It may not be safe to call tcsetpgrp if we've already done so, as at that point we are no
    // longer the controlling process group for the terminal and no longer have permission to set
    // the process group that is in control, causing tcsetpgrp to return EPERM, even though that's
    // not the documented behavior in tcsetpgrp(3), which instead says other bad things will happen
    // (it says SIGTTOU will be sent to all members of the background *calling* process group, but
    // it's more complicated than that, SIGTTOU may or may not be sent depending on the TTY
    // configuration and whether or not signal handlers for SIGTTOU are installed. Read:
    // http://curiousthing.org/sigttin-sigttou-deep-dive-linux In all cases, our goal here was just
    // to hand over control of the terminal to this process group, which is a no-op if it's already
    // been done.
    if (j->pgid == INVALID_PID || tcgetpgrp(STDIN_FILENO) == j->pgid) {
        debug(4, L"Process group %d already has control of terminal\n", j->pgid);
    } else {
        debug(4,
              L"Attempting to bring process group to foreground via tcsetpgrp for job->pgid %d\n",
              j->pgid);

        // The tcsetpgrp(2) man page says that EPERM is thrown if "pgrp has a supported value, but
        // is not the process group ID of a process in the same session as the calling process."
        // Since we _guarantee_ that this isn't the case (the child calls setpgid before it calls
        // SIGSTOP, and the child was created in the same session as us), it seems that EPERM is
        // being thrown because of an caching issue - the call to tcsetpgrp isn't seeing the
        // newly-created process group just yet. On this developer's test machine (WSL running Linux
        // 4.4.0), EPERM does indeed disappear on retry. The important thing is that we can
        // guarantee the process isn't going to exit while we wait (which would cause us to possibly
        // block indefinitely).
        while (tcsetpgrp(STDIN_FILENO, j->pgid) != 0) {
            debug(3, "tcsetpgrp failed");
            bool pgroup_terminated = false;
            if (errno == EINTR) {
                ;  // Always retry on EINTR, see comments in tcsetattr EINTR code below.
            } else if (errno == EINVAL) {
                // OS X returns EINVAL if the process group no longer lives. Probably other OSes,
                // too. Unlike EPERM below, EINVAL can only happen if the process group has
                // terminated.
                pgroup_terminated = true;
            } else if (errno == EPERM) {
                // Retry so long as this isn't because the process group is dead.
                int wait_result = waitpid(-1 * j->pgid, &wait_result, WNOHANG);
                if (wait_result == -1) {
                    // Note that -1 is technically an "error" for waitpid in the sense that an
                    // invalid argument was specified because no such process group exists any
                    // longer. This is the observed behavior on Linux 4.4.0. a "success" result
                    // would mean processes from the group still exist but is still running in some
                    // state or the other.
                    pgroup_terminated = true;
                } else {
                    // Debug the original tcsetpgrp error (not the waitpid errno) to the log, and
                    // then retry until not EPERM or the process group has exited.
                    debug(2, L"terminal_give_to_job(): EPERM.\n", j->pgid);
                }
            } else {
                if (errno == ENOTTY) redirect_tty_output();
                debug(1, _(L"Could not send job %d ('%ls') with pgid %d to foreground"), j->job_id,
                      j->command_wcstr(), j->pgid);
                wperror(L"tcsetpgrp");
                signal_unblock();
                return false;
            }

            if (pgroup_terminated) {
                // All processes in the process group has exited. Since we force all child procs to
                // SIGSTOP on startup, the only way that can happen is if the very last process in
                // the group terminated, and didn't need to access the terminal, otherwise it would
                // have hung waiting for terminal IO (SIGTTIN). We can ignore this.
                debug(3, L"tcsetpgrp called but process group %d has terminated.\n", j->pgid);
                mark_job_complete(j);
                signal_unblock();
                return true;
            }
        }
    }

    if (cont) {
        int result = -1;
        // TODO: Remove this EINTR loop since we have blocked all signals and thus cannot be
        // interrupted. I'm leaving it in place because all of the logic involving controlling
        // terminal management is more than a little opaque and smacks of voodoo programming.
        errno = EINTR;
        while (result == -1 && errno == EINTR) {
            result = tcsetattr(STDIN_FILENO, TCSADRAIN, &j->tmodes);
        }
        if (result == -1) {
            if (errno == ENOTTY) redirect_tty_output();
            debug(1, _(L"Could not send job %d ('%ls') to foreground"), j->job_id,
                  j->command_wcstr());
            wperror(L"tcsetattr");
            signal_unblock();
            return false;
        }
    }

    signal_unblock();
    return true;
}

pid_t terminal_acquire_before_builtin(int job_pgid) {
    pid_t selfpid = getpid();
    pid_t current_owner = tcgetpgrp(STDIN_FILENO);
    if (current_owner >= 0 && current_owner != selfpid && current_owner == job_pgid) {
        if (tcsetpgrp(STDIN_FILENO, selfpid) == 0) {
            return current_owner;
        }
    }
    return -1;
}

/// Returns control of the terminal to the shell, and saves the terminal attribute state to the job,
/// so that we can restore the terminal ownership to the job at a later time.
static bool terminal_return_from_job(job_t *j) {
    errno = 0;
    if (j->pgid == 0) {
        debug(2, "terminal_return_from_job() returning early due to no process group");
        return true;
    }

    signal_block();
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        if (errno == ENOTTY) redirect_tty_output();
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcsetpgrp");
        signal_unblock();
        return false;
    }

    // Save jobs terminal modes.
    if (tcgetattr(STDIN_FILENO, &j->tmodes)) {
        if (errno == EIO) redirect_tty_output();
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcgetattr");
        signal_unblock();
        return false;
    }

// Disabling this per
// https://github.com/adityagodbole/fish-shell/commit/9d229cd18c3e5c25a8bd37e9ddd3b67ddc2d1b72 On
// Linux, 'cd . ; ftp' prevents you from typing into the ftp prompt. See
// https://github.com/fish-shell/fish-shell/issues/121
#if 0
    // Restore the shell's terminal modes.
    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_modes) == -1) {
        if (errno == EIO) redirect_tty_output();
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcsetattr");
        return false;
    }
#endif

    signal_unblock();
    return true;
}

void job_continue(job_t *j, bool cont) {
    // Put job first in the job list.
    job_promote(j);
    j->set_flag(JOB_NOTIFIED, false);

    CHECK_BLOCK();
    debug(4, L"%ls job %d, gid %d (%ls), %ls, %ls", cont ? L"Continue" : L"Start", j->job_id,
          j->pgid, j->command_wcstr(), job_is_completed(j) ? L"COMPLETED" : L"UNCOMPLETED",
          is_interactive ? L"INTERACTIVE" : L"NON-INTERACTIVE");

    if (!job_is_completed(j)) {
        if (j->get_flag(JOB_TERMINAL) && j->get_flag(JOB_FOREGROUND)) {
            // Put the job into the foreground. Hack: ensure that stdin is marked as blocking first
            // (issue #176).
            make_fd_blocking(STDIN_FILENO);
            if (!terminal_give_to_job(j, cont)) return;
        }

        // Send the job a continue signal, if necessary.
        if (cont) {
            for (process_ptr_t &p : j->processes) p->stopped = false;

            if (j->get_flag(JOB_CONTROL)) {
                if (killpg(j->pgid, SIGCONT)) {
                    wperror(L"killpg (SIGCONT)");
                    return;
                }
            } else {
                for (const process_ptr_t &p : j->processes) {
                    if (kill(p->pid, SIGCONT) < 0) {
                        wperror(L"kill (SIGCONT)");
                        return;
                    }
                }
            }
        }

        if (j->get_flag(JOB_FOREGROUND)) {
            // Look for finished processes first, to avoid select() if it's already done.
            process_mark_finished_children(false);

            // Wait for job to report.
            while (!reader_exit_forced() && !job_is_stopped(j) && !job_is_completed(j)) {
                switch (select_try(j)) {
                    case 1: {
                        // debug(1, L"select_try() 1" );
                        read_try(j);
                        process_mark_finished_children(false);
                        break;
                    }
                    case 0: {
                        // debug(1, L"select_try() 0" );
                        // No FDs are ready. Look for finished processes.
                        process_mark_finished_children(true);
                        break;
                    }
                    case -1: {
                        // debug(1, L"select_try() -1" );
                        // If there is no funky IO magic, we can use waitpid instead of handling
                        // child deaths through signals. This gives a rather large speed boost (A
                        // factor 3 startup time improvement on my 300 MHz machine) on short-lived
                        // jobs.
                        //
                        // This will return early if we get a signal, like SIGHUP.
                        process_mark_finished_children(true);
                        break;
                    }
                    default: {
                        DIE("unexpected return value from select_try()");
                        break;
                    }
                }
            }
        }
    }

    if (j->get_flag(JOB_FOREGROUND)) {
        if (job_is_completed(j)) {
            // It's possible that the job will produce output and exit before we've even read from
            // it.
            //
            // We'll eventually read the output, but it may be after we've executed subsequent calls
            // This is why my prompt colors kept getting screwed up - the builtin echo calls
            // were sometimes having their output combined with the set_color calls in the wrong
            // order!
            read_try(j);

            const std::unique_ptr<process_t> &p = j->processes.back();

            // Mark process status only if we are in the foreground and the last process in a pipe,
            // and it is not a short circuited builtin.
            if ((WIFEXITED(p->status) || WIFSIGNALED(p->status)) && p->pid) {
                int status = proc_format_status(p->status);
                // fwprintf(stdout, L"setting status %d for %ls\n", job_get_flag( j, JOB_NEGATE
                // )?!status:status, j->command);
                proc_set_last_status(j->get_flag(JOB_NEGATE) ? !status : status);
            }
        }

        // Put the shell back in the foreground.
        if (j->get_flag(JOB_TERMINAL) && j->get_flag(JOB_FOREGROUND)) {
            terminal_return_from_job(j);
        }
    }
}

int proc_format_status(int status) {
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return status;
}

void proc_sanity_check() {
    const job_t *fg_job = NULL;

    job_iterator_t jobs;
    while (const job_t *j = jobs.next()) {
        if (!j->get_flag(JOB_CONSTRUCTED)) continue;

        // More than one foreground job?
        if (j->get_flag(JOB_FOREGROUND) && !(job_is_stopped(j) || job_is_completed(j))) {
            if (fg_job) {
                debug(0, _(L"More than one job in foreground: job 1: '%ls' job 2: '%ls'"),
                      fg_job->command_wcstr(), j->command_wcstr());
                sanity_lose();
            }
            fg_job = j;
        }

        for (const process_ptr_t &p : j->processes) {
            // Internal block nodes do not have argv - see issue #1545.
            bool null_ok = (p->type == INTERNAL_BLOCK_NODE);
            validate_pointer(p->get_argv(), _(L"Process argument list"), null_ok);
            validate_pointer(p->argv0(), _(L"Process name"), null_ok);

            if ((p->stopped & (~0x00000001)) != 0) {
                debug(0, _(L"Job '%ls', process '%ls' has inconsistent state \'stopped\'=%d"),
                      j->command_wcstr(), p->argv0(), p->stopped);
                sanity_lose();
            }

            if ((p->completed & (~0x00000001)) != 0) {
                debug(0, _(L"Job '%ls', process '%ls' has inconsistent state \'completed\'=%d"),
                      j->command_wcstr(), p->argv0(), p->completed);
                sanity_lose();
            }
        }
    }
}

void proc_push_interactive(int value) {
    ASSERT_IS_MAIN_THREAD();
    int old = is_interactive;
    interactive_stack.push_back(is_interactive);
    is_interactive = value;
    if (old != value) signal_set_handlers();
}

void proc_pop_interactive() {
    ASSERT_IS_MAIN_THREAD();
    int old = is_interactive;
    is_interactive = interactive_stack.back();
    interactive_stack.pop_back();
    if (is_interactive != old) signal_set_handlers();
}

pid_t proc_wait_any() {
    int pid_status;
    pid_t pid = waitpid(-1, &pid_status, WUNTRACED);
    if (pid == -1) return -1;
    handle_child_status(pid, pid_status);
    process_clean_after_marking(is_interactive);
    return pid;
}
