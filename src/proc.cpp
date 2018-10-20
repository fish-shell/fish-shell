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

void job_t::promote() {
    ASSERT_IS_MAIN_THREAD();
    parser_t::principal_parser().job_promote(this);
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

job_t *job_t::from_job_id(job_id_t id) {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_get(id);
}

job_t *job_t::from_pid(pid_t pid) {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_get_from_pid(pid);
}

/// Return true if all processes in the job have stopped or completed.
bool job_t::is_stopped() const {
    for (const process_ptr_t &p : processes) {
        if (!p->completed && !p->stopped) {
            return false;
        }
    }
    return true;
}

/// Return true if the last processes in the job has completed.
bool job_t::is_completed() const {
    assert(!processes.empty());
    for (const process_ptr_t &p : processes) {
        if (!p->completed) {
            return false;
        }
    }
    return true;
}

void job_t::set_flag(job_flag_t flag, bool set) { this->flags.set(flag, set); }

bool job_t::get_flag(job_flag_t flag) const { return this->flags.get(flag); }

bool job_t::signal(int signal) {
    // Presumably we are distinguishing between the two cases below because we do
    // not want to send ourselves the signal in question in case the job shares
    // a pgid with the shell.

    if (pgid != getpgrp()) {
        if (killpg(pgid, signal) == -1) {
            char buffer[512];
            sprintf(buffer, "killpg(%d, %s)", pgid, strsignal(signal));
            wperror(str2wcstring(buffer).c_str());
            return false;
        }
    } else {
        for (const auto &p : processes) {
            if (!p->completed && p->pid && kill(p->pid, signal) == -1) {
                return false;
            }
        }
    }

    return true;
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
    : block_io(std::move(bio)), pgid(INVALID_PID), tmodes(), job_id(jobid), flags{} {}

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

/// A static value tracking how many SIGCHLDs we have seen, which is used in a heurstic to
/// determine if we should call waitpid() at all in `process_mark_finished_children`.
static volatile process_generation_count_t s_sigchld_generation_cnt = 0;

/// See if any children of a fully constructed job have exited or been killed, and mark them
/// accordingly. We cannot reap just any child that's exited, (as in, `waitpid(-1,…`) since
/// that may reap a pgrp leader that has exited but in a job with another process that has yet to
/// launch and join its pgrp (#5219).
/// \param block_on_fg when true, blocks waiting for the foreground job to finish.
/// \return whether the operation completed without incident
static bool process_mark_finished_children(bool block_on_fg) {
    ASSERT_IS_MAIN_THREAD();

    // This is always called from the main thread (and not forked), so we can cache this value.
    static pid_t shell_pgid = getpgrp();

    // We can't always use SIGCHLD to determine if waitpid() should be called since it is not
    // strictly one-SIGCHLD-per-one-child-exited (i.e. multiple exits can share a SIGCHLD call) and
    // we a) return immediately the first time a dead child is reaped, b) explicitly skip over jobs
    // that aren't yet fully constructed, so it's possible that we can get SIGCHLD and even find a
    // killed child in the jobs we are reaping, but also have an exited child process in a job that
    // hasn't been fully constructed yet - which means we can end up never knowing about the exited
    // child process in that job if we use SIGCHLD count as the only metric for whether or not
    // waitpid() is called.
    // Without this optimization, the slowdown caused by calling waitpid() even just once each time
    // `process_mark_finished_children()` is called is rather obvious (see the performance-related
    // discussion in #5219), making it worth the complexity of this heuristic.

    /// Tracks whether or not we received SIGCHLD without checking all jobs (due to jobs under
    /// construction), forcing a full waitpid loop.
    static bool dirty_state = true;
    static process_generation_count_t last_sigchld_count = -1;

    // If the last time that we received a SIGCHLD we did not waitpid all jobs, we cannot early out.
    if (!dirty_state && last_sigchld_count == s_sigchld_generation_cnt) {
        // If we have foreground jobs, we need to block on them below
        if (!block_on_fg)
        {
            // We can assume that no children have exited and that all waitpid calls with
            // WNOHANG below will confirm that.
            return true;
        }
    }

    last_sigchld_count = s_sigchld_generation_cnt;
    bool jobs_skipped = false;
    bool has_error = false;
    job_t *job_fg = nullptr;

    // Reap only processes belonging to fully-constructed jobs to prevent reaping of processes
    // before others in the same process group have a chance to join their pgrp.
    job_iterator_t jobs;
    while (auto j = jobs.next()) {
        // (A job can have pgrp INVALID_PID if it consists solely of builtins that perform no IO)
        if (j->pgid == INVALID_PID || !j->is_constructed()) {
            debug(5, "Skipping wait on incomplete job %d (%ls)", j->job_id, j->preview().c_str());
            jobs_skipped = true;
            continue;
        }

        if (j != job_fg && j->is_foreground() && !j->is_stopped() && !j->is_completed()) {
            // Ignore jobs created via function evaluation in this sanity check
            if (!job_fg || (!job_fg->get_flag(job_flag_t::NESTED) && !j->get_flag(job_flag_t::NESTED))) {
                assert(job_fg == nullptr && "More than one active, fully-constructed foreground job!");
            }
            job_fg = j;
        }

        // Whether we will wait for uncompleted processes depends on the combination of `block_on_fg` and the
        // nature of the process. Default is WNOHANG, but if foreground, constructed, not stopped, *and*
        // block_on_fg is true, then no WNOHANG (i.e. "HANG").
        int options = WUNTRACED | WNOHANG;

        // We should never block twice in the same go, as `waitpid()' returning could mean one
        // process completed or many, and there is a race condition when calling `waitpid()` after
        // the process group exits having reaped all children and terminated the process group and
        // when a subsequent call to `waitpid()` for the same process group returns immediately if
        // that process group no longer exists. i.e. it's possible for all processes to have exited
        // but the process group to remain momentarily valid, in which case calling `waitpid()`
        // without WNOHANG can cause an infinite wait. Additionally, only wait on external jobs that
        // spawned new process groups (i.e. JOB_CONTROL). We do not break or return on error as we
        // wait on only one pgrp at a time and we need to check all pgrps before returning, but we
        // never wait/block on fg processes after an error has been encountered to give ourselves
        // (elsewhere) a chance to handle the fallout from process termination, etc.
        if (!has_error && block_on_fg && j->pgid != shell_pgid && j == job_fg
                && j->get_flag(job_flag_t::JOB_CONTROL)) {
            debug(4, "Waiting on processes from foreground job %d", job_fg->pgid);
            options &= ~WNOHANG;
        }

        bool wait_by_process = j->get_flag(job_flag_t::WAIT_BY_PROCESS);
        process_list_t::iterator process = j->processes.begin();
        // waitpid(2) returns 1 process each time, we need to keep calling it until we've reaped all
        // children of the pgrp in question or else we can't reset the dirty_state flag. In all
        // cases, calling waitpid(2) is faster than potentially calling select_try() on a process
        // that has exited, which will force us to wait the full timeout before coming back here and
        // calling waitpid() again.
        while (true) {
            int status;
            pid_t pid;

            if (wait_by_process) {
                // If the evaluation of a function resulted in the sharing of a pgroup between the
                // real job and the job that shouldn't have been created as a separate job AND the
                // parent job is still under construction (which is the case when continue_job() is
                // first called on the child job during the recursive call to exec_job() before the
                // parent job has been fully constructed), we need to call waitpid(2) on the
                // individual processes of the child job instead of using a catch-all waitpid(2)
                // call on the job's process group.
                if (process == j->processes.end()) {
                    break;
                }
                assert((*process)->pid != INVALID_PID && "Waiting by process on an invalid PID!");
                pid = waitpid((*process)->pid, &status, options);
                process++;
            } else {
                // A negative PID passed in to `waitpid()` means wait on any child in that process group
                pid = waitpid(-1 * j->pgid, &status, options);
            }

            // Never make two calls to waitpid(2) without WNOHANG (i.e. with "HANG") in a row,
            // because we might wait on a non-stopped job that becomes stopped, but we don't refresh
            // our view of the process state before calling waitpid(2) again here.
            options |= WNOHANG;

            if (pid > 0) {
                // A child process has been reaped
                handle_child_status(pid, status);
            }
            else if (pid == 0 || errno == ECHILD) {
                // No killed/dead children in this particular process group
                if (!wait_by_process) {
                    break;
                }
            } else {
                // pid < 0 indicates an error. One likely failure is ECHILD (no children), which is not
                // an error and is ignored. The other likely failure is EINTR, which means we got a
                // signal, which is considered an error. We absolutely do not break or return on error,
                // as we need to iterate over all constructed jobs but we only call waitpid for one pgrp
                // at a time. We do bypass future waits in case of error, however.
                has_error = true;
                wperror(L"waitpid in process_mark_finished_children");
                break;
            }
        }
    }

    // Yes, the below can be collapsed to a single line, but it's worth being explicit about it with
    // the comments. Fret not, the compiler will optimize it. (It better!)
    if (jobs_skipped) {
        // We received SIGCHLD but were not able to definitely say whether or not all children were
        // reaped.
        dirty_state = true;
    }
    else {
        // We can safely assume that no SIGCHLD means we can just return next time around
        dirty_state = false;
    }

    return !has_error;
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
        if ((!j->get_flag(job_flag_t::SKIP_NOTIFICATION)) && (!interactive) &&
            (!j->is_foreground())) {
            continue;
        }

        for (const process_ptr_t &p : j->processes) {
            int s;
            if (!p->completed) continue;

            if (!p->pid) continue;

            s = p->status;

            // TODO: The generic process-exit event is useless and unused.
            // Remove this in future.
            // Update: This event is used for cleaning up the psub temporary files and folders.
            // Removing it breaks the psub tests as a result.
            proc_fire_event(L"PROCESS_EXIT", EVENT_EXIT, p->pid,
                            (WIFSIGNALED(s) ? -1 : WEXITSTATUS(s)));

            // Ignore signal SIGPIPE.We issue it ourselves to the pipe writer when the pipe reader
            // dies.
            if (!WIFSIGNALED(s) || WTERMSIG(s) == SIGPIPE) {
                continue;
            }

            // Handle signals other than SIGPIPE.
            int proc_is_job = (p->is_first_in_job && p->is_last_in_job);
            if (proc_is_job) j->set_flag(job_flag_t::NOTIFIED, true);
            // Always report crashes.
            if (j->get_flag(job_flag_t::SKIP_NOTIFICATION) &&
                !contains(crashsignals, WTERMSIG(p->status))) {
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
            if (WTERMSIG(p->status) != SIGINT || !j->is_foreground()) {
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
        if (j->is_completed()) {
            if (!j->is_foreground() && !j->get_flag(job_flag_t::NOTIFIED) &&
                !j->get_flag(job_flag_t::SKIP_NOTIFICATION)) {
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
        } else if (j->is_stopped() && !j->get_flag(job_flag_t::NOTIFIED)) {
            // Notify the user about newly stopped jobs.
            if (!j->get_flag(job_flag_t::SKIP_NOTIFICATION)) {
                format_job_info(j, JOB_STOPPED);
                found = 1;
            }
            j->set_flag(job_flag_t::NOTIFIED, true);
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
/// \return the status of the select operation
static select_try_t select_try(job_t *j) {
    fd_set fds;
    int maxfd = -1;

    FD_ZERO(&fds);

    const io_chain_t chain = j->all_io_redirections();
    for (const auto &io : chain) {
        if (io->io_mode == IO_BUFFER) {
            auto io_pipe = static_cast<const io_pipe_t *>(io.get());
            int fd = io_pipe->pipe_fd[0];
            FD_SET(fd, &fds);
            maxfd = std::max(maxfd, fd);
            debug(4, L"select_try on fd %d", fd);
        }
    }

    if (maxfd >= 0) {
        struct timeval timeout;

        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;

        int retval = select(maxfd + 1, &fds, 0, 0, &timeout);
        if (retval == 0) {
            debug(4, L"select_try hit timeout");
            return select_try_t::TIMEOUT;
        }
        return select_try_t::DATA_READY;
    }

    return select_try_t::IO_ERROR;
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
        debug(4, L"proc::read_try('%ls')", j->command_wcstr());
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

// Return control of the terminal to a job's process group. restore_attrs is true if we are restoring
// a previously-stopped job, in which case we need to restore terminal attributes.
bool terminal_give_to_job(const job_t *j, bool restore_attrs) {
    if (j->pgid == 0) {
        debug(2, "terminal_give_to_job() returning early due to no process group");
        return true;
    }

    // RAII wrappers must have a name so that their scope is tied to the function as it is legal for
    // the compiler to construct and then immediately deconstruct unnamed objects otherwise.
    signal_block_t signal_block;

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
            debug(3, "tcsetpgrp failed: %d", errno);

            bool pgroup_terminated = false;
            // No need to test for EINTR as we are blocking signals
            if (errno == EINVAL) {
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
                    continue;
                }
            } else {
                if (errno == ENOTTY) {
                    redirect_tty_output();
                }
                debug(1, _(L"Could not send job %d ('%ls') with pgid %d to foreground"),
                            j->job_id, j->command_wcstr(), j->pgid);
                wperror(L"tcsetpgrp");
                return false;
            }

            if (pgroup_terminated) {
                // All processes in the process group has exited.
                // Since we delay reaping any processes in a process group until all members of that
                // job/group have been started, the only way this can happen is if the very last process in
                // the group terminated and didn't need to access the terminal, otherwise it would
                // have hung waiting for terminal IO (SIGTTIN). We can safely ignore this.
                debug(3, L"tcsetpgrp called but process group %d has terminated.\n", j->pgid);
                mark_job_complete(j);
                return true;
            }

            break;
        }
    }

    if (restore_attrs) {
        auto result = tcsetattr(STDIN_FILENO, TCSADRAIN, &j->tmodes);
        if (result == -1) {
            // No need to test for EINTR and retry since we have blocked all signals
            if (errno == ENOTTY) {
                redirect_tty_output();
            }

            debug(1, _(L"Could not send job %d ('%ls') to foreground"), j->job_id, j->preview().c_str());
            wperror(L"tcsetattr");
            return false;
        }
    }

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

void job_t::continue_job(bool send_sigcont) {
    // Put job first in the job list.
    promote();
    set_flag(job_flag_t::NOTIFIED, false);

    debug(4, L"%ls job %d, gid %d (%ls), %ls, %ls", send_sigcont ? L"Continue" : L"Start", job_id,
          pgid, command_wcstr(), is_completed() ? L"COMPLETED" : L"UNCOMPLETED",
          is_interactive ? L"INTERACTIVE" : L"NON-INTERACTIVE");

    // Make sure we retake control of the terminal before leaving this function.
    bool term_transferred = false;
    cleanup_t take_term_back([&]() {
            if (term_transferred) {
                terminal_return_from_job(this);
            }
        });

    bool read_attempted = false;
    if (!is_completed()) {
        if (get_flag(job_flag_t::TERMINAL) && is_foreground()) {
            // Put the job into the foreground and give it control of the terminal.
            // Hack: ensure that stdin is marked as blocking first (issue #176).
            make_fd_blocking(STDIN_FILENO);
            if (!terminal_give_to_job(this, send_sigcont)) {
                // This scenario has always returned without any error handling. Presumably that is OK.
                return;
            }
            term_transferred = true;
        }

        // If both requested and necessary, send the job a continue signal.
        if (send_sigcont) {
            // This code used to check for JOB_CONTROL to decide between using killpg to signal all
            // processes in the group or iterating over each process in the group and sending the
            // signal individually. job_t::signal() does the same, but uses the shell's own pgroup
            // to make that distinction.
            if (!signal(SIGCONT)) {
                debug(2, "Failed to send SIGCONT to any processes in pgroup %d!", pgid);
                // This returns without bubbling up the error. Presumably that is OK.
                return;
            }

            // reset the status of each process instance
            for (auto &p : processes) {
                p->stopped = false;
            }
        }

        if (is_foreground()) {
            // This is an optimization to not call select_try() in case a process has exited. While
            // it may seem silly, unless there is IO (and there usually isn't in terms of total CPU
            // time), select_try() will wait for 10ms (our timeout) before returning. If during
            // these 10ms a process exited, the shell will basically hang until the timeout happens
            // and we are free to call `process_mark_finished_children()` to discover that fact. By
            // calling it here before calling `select_try()` below, shell responsiveness can be
            // dramatically improved (noticably so, not just "theoretically speaking" per the
            // discussion in #5219).
            process_mark_finished_children(false);

            // If this is a child job and the parent job is still under construction (i.e. job1 |
            // some_func), we can't block on execution of the nested job for `some_func`. Doing
            // so can cause hangs if job1 emits more data than fits in the OS pipe buffer.
            // The test for block_on_fg is this->parent_job.is_constructed(), which coincides
            // with WAIT_BY_PROCESS (which will have to do since we don't store a reference to
            // the parent job in the job_t structure).
            bool block_on_fg = !get_flag(job_flag_t::WAIT_BY_PROCESS);

            // Wait for data to become available or the status of our own job to change
            while (!reader_exit_forced() && !is_stopped() && !is_completed()) {
                auto result = select_try(this);
                read_attempted = true;

                if (result == select_try_t::DATA_READY) {
                    // Read the data that we know is now available, then scan for finished processes
                    // but do not block. We don't block so long as we have IO to process, once the
                    // fd buffers are empty we'll block in the second case below.
                    read_try(this);
                    process_mark_finished_children(false);
                }
                else if (result == select_try_t::TIMEOUT) {
                    // Our select_try() timeout is ~10ms, so this can be EXTREMELY chatty but this
                    // is very useful if trying to debug an unknown hang in fish. Uncomment to see
                    // if we're stuck here.  debug(1, L"select_try: no fds returned valid data
                    // within the timeout" );

                    // No FDs are ready. Look for finished processes instead.
                    process_mark_finished_children(block_on_fg);
                }
                else {
                    // This is easily encountered by simply transferring control of the terminal to
                    // another process, then suspending it. For example, `nvim`, then `ctrl+z`.
                    // Since we are not the foreground process
                    debug(3, L"select_try: interrupted read from job file descriptors" );

                    // This tends to happen when the foreground process has changed, e.g. it was
                    // suspended and control has returned to the shell.
                    process_mark_finished_children(block_on_fg);

                    // If it turns out that we encountered this because the file descriptor we were
                    // reading from has died, process_mark_finished_children() should take care of
                    // changing the status of our is_completed() (assuming it is appropriate to do
                    // so), in which case we will break out of this loop.
                }
            }
        }
    }

    if (is_foreground()) {
        if (is_completed()) {
            // It's possible that the job will produce output and exit before we've even read from
            // it.  In that case, make sure we read that output now, before we've executed any
            // subsequent calls.  This is why prompt colors were getting screwed up - the builtin
            // `echo` calls were sometimes having their output combined with the `set_color` calls
            // in the wrong order!
            if (!read_attempted) {
                read_try(this);
            }

            // Set $status only if we are in the foreground and the last process in the job has finished
            // and is not a short-circuited builtin.
            auto &p = processes.back();
            if ((WIFEXITED(p->status) || WIFSIGNALED(p->status)) && p->pid) {
                int status = proc_format_status(p->status);
                proc_set_last_status(get_flag(job_flag_t::NEGATE) ? !status : status);
            }
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
        if (!j->is_constructed()) continue;

        // More than one foreground job?
        if (j->is_foreground() && !(j->is_stopped() || j->is_completed())) {
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

void hup_background_jobs() {
    job_iterator_t jobs;

    while (job_t *j = jobs.next()) {
        // Make sure we don't try to SIGHUP the calling builtin
        if (j->pgid == INVALID_PID || !j->get_flag(job_flag_t::JOB_CONTROL)) {
            continue;
        }

        if (!j->is_completed()) {
            if (j->is_stopped()) {
                j->signal(SIGCONT);
            }
            j->signal(SIGHUP);
        }
    }
}
