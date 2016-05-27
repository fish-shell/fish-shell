// Utilities for keeping track of jobs, processes and subshells, as well as signal handling
// functions for tracking children. These functions do not themselves launch new processes, the exec
// library will call proc to create representations of the running jobs as needed.
//
// Some of the code in this file is based on code from the Glibc manual.
// IWYU pragma: no_include <__bit_reference>
#include "config.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <memory>
#include <vector>
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <stdbool.h>
#include <sys/time.h>  // IWYU pragma: keep
#include <sys/types.h>
#include <algorithm>  // IWYU pragma: keep

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

bool job_list_is_empty(void) {
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
        printf("%p -> %ls -> (foreground %d, complete %d, stopped %d, constructed %d)\n",
                j, j->command_wcstr(), job_get_flag(j, JOB_FOREGROUND), job_is_completed(j),
                job_is_stopped(j), job_get_flag(j, JOB_CONSTRUCTED));
    }
}
#endif

int is_interactive_session = 0;
int is_subshell = 0;
int is_block = 0;
int is_login = 0;
int is_event = 0;
pid_t proc_last_bg_pid = 0;
int job_control_mode = JOB_CONTROL_INTERACTIVE;
int no_exec = 0;

static int is_interactive = -1;

static bool proc_had_barrier = false;

bool shell_is_interactive(void) {
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

/// Remove job from the job list and free all memory associated with it.
void job_free(job_t *j) {
    job_remove(j);
    delete j;
}

void proc_destroy() {
    job_list_t &jobs = parser_t::principal_parser().job_list();
    while (!jobs.empty()) {
        job_t *job = jobs.front();
        debug(2, L"freeing leaked job %ls", job->command_wcstr());
        job_free(job);
    }
}

void proc_set_last_status(int s) {
    ASSERT_IS_MAIN_THREAD();
    last_status = s;
}

int proc_get_last_status() { return last_status; }

// Basic thread safe job IDs. The vector consumed_job_ids has a true value wherever the job ID
// corresponding to that slot is in use. The job ID corresponding to slot 0 is 1.
static pthread_mutex_t job_id_lock = PTHREAD_MUTEX_INITIALIZER;
static std::vector<bool> consumed_job_ids;

job_id_t acquire_job_id(void) {
    scoped_lock lock(job_id_lock);

    // Find the index of the first 0 slot.
    std::vector<bool>::iterator slot =
        std::find(consumed_job_ids.begin(), consumed_job_ids.end(), false);
    if (slot != consumed_job_ids.end()) {
        // We found a slot. Note that slot 0 corresponds to job ID 1.
        *slot = true;
        return (job_id_t)(slot - consumed_job_ids.begin() + 1);
    }

    // We did not find a slot; create a new slot. The size of the vector is now the job ID
    // (since it is one larger than the slot).
    consumed_job_ids.push_back(true);
    return (job_id_t)consumed_job_ids.size();
}

void release_job_id(job_id_t jid) {
    assert(jid > 0);
    scoped_lock lock(job_id_lock);
    size_t slot = (size_t)(jid - 1), count = consumed_job_ids.size();

    // Make sure this slot is within our vector and is currently set to consumed.
    assert(slot < count);
    assert(consumed_job_ids.at(slot) == true);

    // Clear it and then resize the vector to eliminate unused trailing job IDs.
    consumed_job_ids.at(slot) = false;
    while (count--) {
        if (consumed_job_ids.at(count)) break;
    }
    consumed_job_ids.resize(count + 1);
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
    process_t *p;

    for (p = j->first_process; p; p = p->next) {
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
    assert(j->first_process != NULL);
    bool result = true;
    for (process_t *p = j->first_process; p != NULL; p = p->next) {
        if (!p->completed) {
            result = false;
            break;
        }
    }
    return result;
}

void job_set_flag(job_t *j, unsigned int flag, int set) {
    if (set) {
        j->flags |= flag;
    } else {
        j->flags &= ~flag;
    }
}

int job_get_flag(const job_t *j, unsigned int flag) { return !!(j->flags & flag); }

int job_signal(job_t *j, int signal) {
    pid_t my_pid = getpid();
    int res = 0;

    if (j->pgid != my_pid) {
        res = killpg(j->pgid, SIGHUP);
    } else {
        for (process_t *p = j->first_process; p; p = p->next) {
            if (!p->completed) {
                if (p->pid) {
                    if (kill(p->pid, SIGHUP)) {
                        res = -1;
                        break;
                    }
                }
            }
        }
    }

    return res;
}

/// Store the status of the process pid that was returned by waitpid.
static void mark_process_status(const job_t *j, process_t *p, int status) {
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
        fprintf(stderr, "Process %ld exited abnormally\n", (long)p->pid);
    }
}

void job_mark_process_as_failed(const job_t *job, process_t *p) {
    // The given process failed to even lift off (e.g. posix_spawn failed) and so doesn't have a
    // valid pid. Mark it as dead.
    for (process_t *cursor = p; cursor != NULL; cursor = cursor->next) {
        cursor->completed = 1;
    }
}

/// Handle status update for child \c pid.
///
/// \param pid the pid of the process whose status changes
/// \param status the status as returned by wait
static void handle_child_status(pid_t pid, int status) {
    bool found_proc = false;
    const job_t *j = NULL;
    process_t *p = NULL;

    job_iterator_t jobs;
    while (!found_proc && (j = jobs.next())) {
        process_t *prev = 0;
        for (p = j->first_process; p; p = p->next) {
            if (pid == p->pid) {
                mark_process_status(j, p, status);
                if (p->completed && prev != 0) {
                    if (!prev->completed && prev->pid) {
                        kill(prev->pid, SIGPIPE);
                    }
                }
                found_proc = true;
                break;
            }
            prev = p;
        }
    }

    if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGINT || WTERMSIG(status) == SIGQUIT)) {
        if (!is_interactive_session) {
            struct sigaction act;
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            act.sa_handler = SIG_DFL;
            sigaction(SIGINT, &act, 0);
            sigaction(SIGQUIT, &act, 0);
            kill(getpid(), WTERMSIG(status));
        } else {
            // In an interactive session, tell the principal parser to skip all blocks we're
            // executing so control-C returns control to the user.
            if (p && found_proc) {
                parser_t::skip_all_blocks();
            }
        }
    }

    if (!found_proc) {
        // A child we lost track of? There have been bugs in both subshell handling and in builtin
        // handling that have caused this previously...
    }
    return;
}

process_t::process_t()
    : type(),  // gets set later
      internal_block_node(NODE_OFFSET_INVALID),
      pid(0),
      pipe_write_fd(0),
      pipe_read_fd(0),
      completed(0),
      stopped(0),
      status(0),
      count_help_magic(0),
      next(NULL)
#ifdef HAVE__PROC_SELF_STAT
      ,
      last_time(),
      last_jiffies(0)
#endif
{
}

process_t::~process_t() { delete this->next; }

job_t::job_t(job_id_t jobid, const io_chain_t &bio)
    : block_io(bio), first_process(NULL), pgid(0), tmodes(), job_id(jobid), flags(0) {}

job_t::~job_t() {
    delete first_process;
    release_job_id(job_id);
}

/// Return all the IO redirections. Start with the block IO, then walk over the processes.
io_chain_t job_t::all_io_redirections() const {
    io_chain_t result = this->block_io;
    for (process_t *p = this->first_process; p != NULL; p = p->next) {
        result.append(p->io_chain());
    }
    return result;
}

typedef unsigned int process_generation_count_t;

/// A static value tracking how many SIGCHLDs we have seen. This is only ever modified from within
/// the SIGCHLD signal handler, and therefore does not need atomics or locks.
static volatile process_generation_count_t s_sigchld_generation_count = 0;

/// If we have received a SIGCHLD signal, process any children. If await is false, this returns
/// immediately if no SIGCHLD has been received. If await is true, this waits for one. Returns true
/// if something was processed. This returns the number of children processed, or -1 on error.
static int process_mark_finished_children(bool wants_await) {
    ASSERT_IS_MAIN_THREAD();

    // A static value tracking the SIGCHLD gen count at the time we last processed it. When this is
    // different from s_sigchld_generation_count, it indicates there may be unreaped processes.
    // There may not be if we reaped them via the other waitpid path. This is only ever modified
    // from the main thread, and not from a signal handler.
    static process_generation_count_t s_last_processed_sigchld_generation_count = 0;

    int processed_count = 0;
    bool got_error = false;

    // The critical read. This fetches a value which is only written in the signal handler. This
    // needs to be an atomic read (we'd use sig_atomic_t, if we knew that were unsigned -
    // fortunately aligned unsigned int is atomic on pretty much any modern chip.) It also needs to
    // occur before we start reaping, since the signal handler can be invoked at any point.
    const process_generation_count_t local_count = s_sigchld_generation_count;

    // Determine whether we have children to process. Note that we can't reliably use the difference
    // because a single SIGCHLD may be delivered for multiple children - see #1768. Also if we are
    // awaiting, we always process.
    bool wants_waitpid = wants_await || local_count != s_last_processed_sigchld_generation_count;

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
            pid_t pid = waitpid(-1, &status, options);
            if (pid > 0) {
                // We got a valid pid.
                handle_child_status(pid, status);
                processed_count += 1;
            } else if (pid == 0) {
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
    s_last_processed_sigchld_generation_count = local_count;
    return processed_count;
}

/// This is called from a signal handler. The signal is always SIGCHLD.
void job_handle_signal(int signal, siginfo_t *info, void *con) {
    // This is the only place that this generation count is modified. It's OK if it overflows.
    s_sigchld_generation_count += 1;
}

/// Given a command like "cat file", truncate it to a reasonable length.
static wcstring truncate_command(const wcstring &cmd) {
    const size_t max_len = 32;
    if (cmd.size() <= max_len) {
        // No truncation necessary.
        return cmd;
    }

    // Truncation required.
    const bool ellipsis_is_unicode = (ellipsis_char == L'\x2026');
    const size_t ellipsis_length = ellipsis_is_unicode ? 1 : 3;
    size_t trunc_length = max_len - ellipsis_length;
    // Eat trailing whitespace.
    while (trunc_length > 0 && iswspace(cmd.at(trunc_length - 1))) {
        trunc_length -= 1;
    }
    wcstring result = wcstring(cmd, 0, trunc_length);
    // Append ellipsis.
    if (ellipsis_is_unicode) {
        result.push_back(ellipsis_char);
    } else {
        result.append(L"...");
    }
    return result;
}

/// Format information about job status for the user to look at.
///
/// \param j the job to test
/// \param status a string description of the job exit type
static void format_job_info(const job_t *j, const wchar_t *status, size_t job_count) {
    fwprintf(stdout, L"\r");
    if (job_count == 1) {
        fwprintf(stdout, _(L"\'%ls\' has %ls"), truncate_command(j->command()).c_str(), status);
    } else {
        fwprintf(stdout, _(L"Job %d, \'%ls\' has %ls"), j->job_id,
                 truncate_command(j->command()).c_str(), status);
    }
    fflush(stdout);
    tputs(clr_eol, 1, &writeb);
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

int job_reap(bool interactive) {
    ASSERT_IS_MAIN_THREAD();
    job_t *jnext;
    int found = 0;

    // job_reap may fire an event handler, we do not want to call ourselves recursively (to avoid
    // infinite recursion).
    static bool locked = false;
    if (locked) {
        return 0;
    }
    locked = true;

    process_mark_finished_children(false);

    // Preserve the exit status.
    const int saved_status = proc_get_last_status();

    job_iterator_t jobs;
    const size_t job_count = jobs.count();
    jnext = jobs.next();
    while (jnext) {
        job_t *j = jnext;
        jnext = jobs.next();

        // If we are reaping only jobs who do not need status messages sent to the console, do not
        // consider reaping jobs that need status messages.
        if ((!job_get_flag(j, JOB_SKIP_NOTIFICATION)) && (!interactive) &&
            (!job_get_flag(j, JOB_FOREGROUND))) {
            continue;
        }

        for (process_t *p = j->first_process; p; p = p->next) {
            int s;
            if (!p->completed) continue;

            if (!p->pid) continue;

            s = p->status;

            proc_fire_event(L"PROCESS_EXIT", EVENT_EXIT, p->pid,
                            (WIFSIGNALED(s) ? -1 : WEXITSTATUS(s)));

            if (WIFSIGNALED(s)) {
                // Ignore signal SIGPIPE.We issue it ourselves to the pipe writer when the pipe
                // reader dies.
                if (WTERMSIG(s) != SIGPIPE) {
                    int proc_is_job = ((p == j->first_process) && (p->next == 0));
                    if (proc_is_job) job_set_flag(j, JOB_NOTIFIED, 1);
                    if (!job_get_flag(j, JOB_SKIP_NOTIFICATION)) {
                        // Print nothing if we get SIGINT in the foreground process group, to avoid
                        // spamming obvious stuff on the console (#1119). If we get SIGINT for the
                        // foreground process, assume the user typed ^C and can see it working. It's
                        // possible they didn't, and the signal was delivered via pkill, etc., but
                        // the SIGINT/SIGTERM distinction is precisely to allow INT to be from a UI
                        // and TERM to be programmatic, so this assumption is keeping with the
                        // design of signals. If echoctl is on, then the terminal will have written
                        // ^C to the console. If off, it won't have. We don't echo ^C either way, so
                        // as to respect the user's preference.
                        if (WTERMSIG(p->status) != SIGINT || !job_get_flag(j, JOB_FOREGROUND)) {
                            if (proc_is_job) {
                                // We want to report the job number, unless it's the only job, in
                                // which case we don't need to.
                                const wcstring job_number_desc =
                                    (job_count == 1) ? wcstring()
                                                     : format_string(L"Job %d, ", j->job_id);
                                fwprintf(stdout,
                                         _(L"%ls: %ls\'%ls\' terminated by signal %ls (%ls)"),
                                         program_name, job_number_desc.c_str(),
                                         truncate_command(j->command()).c_str(),
                                         sig2wcs(WTERMSIG(p->status)),
                                         signal_get_desc(WTERMSIG(p->status)));
                            } else {
                                const wcstring job_number_desc =
                                    (job_count == 1) ? wcstring()
                                                     : format_string(L"from job %d, ", j->job_id);
                                fwprintf(stdout, _(L"%ls: Process %d, \'%ls\' %ls\'%ls\' "
                                                   L"terminated by signal %ls (%ls)"),
                                         program_name, p->pid, p->argv0(), job_number_desc.c_str(),
                                         truncate_command(j->command()).c_str(),
                                         sig2wcs(WTERMSIG(p->status)),
                                         signal_get_desc(WTERMSIG(p->status)));
                            }
                            tputs(clr_eol, 1, &writeb);
                            fwprintf(stdout, L"\n");
                        }
                        found = 1;
                    }

                    // Clear status so it is not reported more than once.
                    p->status = 0;
                }
            }
        }

        // If all processes have completed, tell the user the job has completed and delete it from
        // the active job list.
        if (job_is_completed(j)) {
            if (!job_get_flag(j, JOB_FOREGROUND) && !job_get_flag(j, JOB_NOTIFIED) &&
                !job_get_flag(j, JOB_SKIP_NOTIFICATION)) {
                format_job_info(j, _(L"ended"), job_count);
                found = 1;
            }
            proc_fire_event(L"JOB_EXIT", EVENT_EXIT, -j->pgid, 0);
            proc_fire_event(L"JOB_EXIT", EVENT_JOB_ID, j->job_id, 0);

            job_free(j);
        } else if (job_is_stopped(j) && !job_get_flag(j, JOB_NOTIFIED)) {
            // Notify the user about newly stopped jobs.
            if (!job_get_flag(j, JOB_SKIP_NOTIFICATION)) {
                format_job_info(j, _(L"stopped"), job_count);
                found = 1;
            }
            job_set_flag(j, JOB_NOTIFIED, 1);
        }
    }

    if (found) fflush(stdout);

    // Restore the exit status.
    proc_set_last_status(saved_status);

    locked = false;

    return found;
}

#ifdef HAVE__PROC_SELF_STAT

/// Maximum length of a /proc/[PID]/stat filename.
#define FN_SIZE 256

/// Get the CPU time for the specified process.
unsigned long proc_get_jiffies(process_t *p) {
    wchar_t fn[FN_SIZE];

    char state;
    int pid, ppid, pgrp, session, tty_nr, tpgid, exit_signal, processor;

    long int cutime, cstime, priority, nice, placeholder, itrealvalue, rss;
    unsigned long int flags, minflt, cminflt, majflt, cmajflt, utime, stime, starttime, vsize, rlim,
        startcode, endcode, startstack, kstkesp, kstkeip, signal, blocked, sigignore, sigcatch,
        wchan, nswap, cnswap;
    char comm[1024];

    if (p->pid <= 0) return 0;

    swprintf(fn, FN_SIZE, L"/proc/%d/stat", p->pid);

    FILE *f = wfopen(fn, "r");
    if (!f) return 0;

    int count = fscanf(
        f,
        "%d %s %c "
        "%d %d %d "
        "%d %d %lu "

        "%lu %lu %lu "
        "%lu %lu %lu "
        "%ld %ld %ld "

        "%ld %ld %ld "
        "%lu %lu %ld "
        "%lu %lu %lu "

        "%lu %lu %lu "
        "%lu %lu %lu "
        "%lu %lu %lu "

        "%lu %d %d ",

        &pid, comm, &state, &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags,

        &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime, &cutime, &cstime, &priority,

        &nice, &placeholder, &itrealvalue, &starttime, &vsize, &rss, &rlim, &startcode, &endcode,

        &startstack, &kstkesp, &kstkeip, &signal, &blocked, &sigignore, &sigcatch, &wchan, &nswap,

        &cnswap, &exit_signal, &processor);

    // Don't need to check exit status of fclose on read-only streams.
    fclose(f);

    if (count < 17) {
        return 0;
    }

    return utime + stime + cutime + cstime;
}

/// Update the CPU time for all jobs.
void proc_update_jiffies() {
    job_t *job;
    process_t *p;
    job_iterator_t j;

    for (job = j.next(); job; job = j.next()) {
        for (p = job->first_process; p; p = p->next) {
            gettimeofday(&p->last_time, 0);
            p->last_jiffies = proc_get_jiffies(p);
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
            CAST_INIT(const io_pipe_t *, io_pipe, io);
            int fd = io_pipe->pipe_fd[0];
            // fwprintf( stderr, L"fd %d on job %ls\n", fd, j->command );
            FD_SET(fd, &fds);
            maxfd = maxi(maxfd, fd);
            debug(3, L"select_try on %d\n", fd);
        }
    }

    if (maxfd >= 0) {
        int retval;
        struct timeval tv;

        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        retval = select(maxfd + 1, &fds, 0, 0, &tv);
        if (retval == 0) {
            debug(3, L"select_try hit timeout\n");
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
        debug(3, L"proc::read_try('%ls')\n", j->command_wcstr());
        while (1) {
            char b[BUFFER_SIZE];
            long l;

            l = read_blocked(buff->pipe_fd[0], b, BUFFER_SIZE);
            if (l == 0) {
                break;
            } else if (l < 0) {
                if (errno != EAGAIN) {
                    debug(1, _(L"An error occured while reading output from code block"));
                    wperror(L"read_try");
                }
                break;
            } else {
                buff->out_buffer_append(b, l);
            }
        }
    }
}

/// Give ownership of the terminal to the specified job.
///
/// \param j The job to give the terminal to.
/// \param cont If this variable is set, we are giving back control to a job that has previously
/// been stopped. In that case, we need to set the terminal attributes to those saved in the job.
static bool terminal_give_to_job(job_t *j, int cont) {
    if (tcsetpgrp(0, j->pgid)) {
        debug(1, _(L"Could not send job %d ('%ls') to foreground"), j->job_id, j->command_wcstr());
        wperror(L"tcsetpgrp");
        return false;
    }

    if (cont) {
        if (tcsetattr(0, TCSADRAIN, &j->tmodes)) {
            debug(1, _(L"Could not send job %d ('%ls') to foreground"), j->job_id,
                  j->command_wcstr());
            wperror(L"tcsetattr");
            return false;
        }
    }
    return true;
}

/// Returns control of the terminal to the shell, and saves the terminal attribute state to the job,
/// so that we can restore the terminal ownership to the job at a later time.
static int terminal_return_from_job(job_t *j) {
    if (tcsetpgrp(0, getpgrp())) {
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcsetpgrp");
        return 0;
    }

    /*
       Save jobs terminal modes.
    */
    if (tcgetattr(0, &j->tmodes)) {
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcgetattr");
        return 0;
    }

// Disabling this per
// https://github.com/adityagodbole/fish-shell/commit/9d229cd18c3e5c25a8bd37e9ddd3b67ddc2d1b72 On
// Linux, 'cd . ; ftp' prevents you from typing into the ftp prompt. See
// https://github.com/fish-shell/fish-shell/issues/121
#if 0
    // Restore the shell's terminal modes.
    if (tcsetattr(0, TCSADRAIN, &shell_modes)) {
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcsetattr");
        return 0;
    }
#endif

    return 1;
}

void job_continue(job_t *j, bool cont) {
    // Put job first in the job list.
    job_promote(j);
    job_set_flag(j, JOB_NOTIFIED, 0);

    CHECK_BLOCK();

    debug(4, L"Continue job %d, gid %d (%ls), %ls, %ls", j->job_id, j->pgid, j->command_wcstr(),
          job_is_completed(j) ? L"COMPLETED" : L"UNCOMPLETED",
          is_interactive ? L"INTERACTIVE" : L"NON-INTERACTIVE");

    if (!job_is_completed(j)) {
        if (job_get_flag(j, JOB_TERMINAL) && job_get_flag(j, JOB_FOREGROUND)) {
            // Put the job into the foreground. Hack: ensure that stdin is marked as blocking first
            // (issue #176).
            make_fd_blocking(STDIN_FILENO);

            signal_block();

            bool ok = terminal_give_to_job(j, cont);

            signal_unblock();

            if (!ok) return;
        }

        // Send the job a continue signal, if necessary.
        if (cont) {
            process_t *p;

            for (p = j->first_process; p; p = p->next) p->stopped = 0;

            if (job_get_flag(j, JOB_CONTROL)) {
                if (killpg(j->pgid, SIGCONT)) {
                    wperror(L"killpg (SIGCONT)");
                    return;
                }
            } else {
                for (p = j->first_process; p; p = p->next) {
                    if (kill(p->pid, SIGCONT) < 0) {
                        wperror(L"kill (SIGCONT)");
                        return;
                    }
                }
            }
        }

        if (job_get_flag(j, JOB_FOREGROUND)) {
            // Look for finished processes first, to avoid select() if it's already done.
            process_mark_finished_children(false);

            // Wait for job to report.
            while (!reader_exit_forced() && !job_is_stopped(j) && !job_is_completed(j)) {
                // debug( 1, L"select_try()" );
                switch (select_try(j)) {
                    case 1: {
                        read_try(j);
                        process_mark_finished_children(false);
                        break;
                    }

                    case 0: {
                        // No FDs are ready. Look for finished processes.
                        process_mark_finished_children(false);
                        break;
                    }

                    case -1: {
                        // If there is no funky IO magic, we can use waitpid instead of handling
                        // child deaths through signals. This gives a rather large speed boost (A
                        // factor 3 startup time improvement on my 300 MHz machine) on short-lived
                        // jobs.
                        //
                        // This will return early if we get a signal, like SIGHUP.
                        process_mark_finished_children(true);
                        break;
                    }
                }
            }
        }
    }

    if (job_get_flag(j, JOB_FOREGROUND)) {
        if (job_is_completed(j)) {
            // It's possible that the job will produce output and exit before we've even read from
            // it.
            //
            // We'll eventually read the output, but it may be after we've executed subsequent calls
            // This is why my prompt colors kept getting screwed up - the builtin echo calls
            // were sometimes having their output combined with the set_color calls in the wrong
            // order!
            read_try(j);

            process_t *p = j->first_process;
            while (p->next) p = p->next;

            if (WIFEXITED(p->status) || WIFSIGNALED(p->status)) {
                // Mark process status only if we are in the foreground and the last process in a
                // pipe, and it is not a short circuited builtin.
                if (p->pid) {
                    int status = proc_format_status(p->status);
                    // wprintf(L"setting status %d for %ls\n", job_get_flag( j, JOB_NEGATE
                    // )?!status:status, j->command);
                    proc_set_last_status(job_get_flag(j, JOB_NEGATE) ? !status : status);
                }
            }
        }

        // Put the shell back in the foreground.
        if (job_get_flag(j, JOB_TERMINAL) && job_get_flag(j, JOB_FOREGROUND)) {
            int ok;

            signal_block();

            ok = terminal_return_from_job(j);

            signal_unblock();

            if (!ok) return;
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
    job_t *j;
    job_t *fg_job = 0;

    job_iterator_t jobs;
    while ((j = jobs.next())) {
        process_t *p;

        if (!job_get_flag(j, JOB_CONSTRUCTED)) continue;

        validate_pointer(j->first_process, _(L"Process list pointer"), 0);

        // More than one foreground job?
        if (job_get_flag(j, JOB_FOREGROUND) && !(job_is_stopped(j) || job_is_completed(j))) {
            if (fg_job != 0) {
                debug(0, _(L"More than one job in foreground: job 1: '%ls' job 2: '%ls'"),
                      fg_job->command_wcstr(), j->command_wcstr());
                sanity_lose();
            }
            fg_job = j;
        }

        p = j->first_process;
        while (p) {
            // Internal block nodes do not have argv - see issue #1545.
            bool null_ok = (p->type == INTERNAL_BLOCK_NODE);
            validate_pointer(p->get_argv(), _(L"Process argument list"), null_ok);
            validate_pointer(p->argv0(), _(L"Process name"), null_ok);
            validate_pointer(p->next, _(L"Process list pointer"), true);

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

            p = p->next;
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
