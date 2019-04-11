// Utilities for keeping track of jobs, processes and subshells, as well as signal handling
// functions for tracking children. These functions do not themselves launch new processes, the exec
// library will call proc to create representations of the running jobs as needed.
//
// Some of the code in this file is based on code from the Glibc manual.
// IWYU pragma: no_include <__bit_reference>
#include "config.h"

#include <atomic>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <cwchar>
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
#include "wutil.h"  // IWYU pragma: keep

/// Statuses of last job's processes to exit - ensure we start off with one entry of 0.
static owning_lock<statuses_t> last_statuses{statuses_t::just(0)};

/// The signals that signify crashes to us.
static const int crashsignals[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGSYS};

bool job_list_is_empty() {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_list().empty();
}

job_list_t& jobs() {
    ASSERT_IS_MAIN_THREAD();
    return parser_t::principal_parser().job_list();
}

bool is_interactive_session = false;
bool is_subshell = false;
bool is_block = false;
bool is_breakpoint = false;
bool is_login = false;
int is_event = 0;
job_control_t job_control_mode = job_control_t::interactive;
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

/// A stack containing the values of is_interactive. Used by proc_push_interactive and
/// proc_pop_interactive.
static std::vector<int> interactive_stack;

void proc_init() { proc_push_interactive(0); }

void job_t::promote() {
    ASSERT_IS_MAIN_THREAD();
    parser_t::principal_parser().job_promote(this);
}

void proc_destroy() {
    for (const auto &job : jobs()) {
        debug(2, L"freeing leaked job %ls", job->command_wcstr());
    }
    jobs().clear();
}

void proc_set_last_statuses(statuses_t s) {
    ASSERT_IS_MAIN_THREAD();
    *last_statuses.acquire() = std::move(s);
}

int proc_get_last_status() { return last_statuses.acquire()->status; }

statuses_t proc_get_last_statuses() { return *last_statuses.acquire(); }

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

bool job_t::job_chain_is_fully_constructed() const {
    const job_t *cursor = this;
    while (cursor) {
        if (!cursor->is_constructed()) return false;
        cursor = cursor->get_parent().get();
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

statuses_t job_t::get_statuses() const {
    statuses_t st{};
    st.pipestatus.reserve(processes.size());
    for (const auto &p : processes) {
        st.pipestatus.push_back(p->status.status_value());
    }
    int laststatus = st.pipestatus.back();
    st.status = (get_flag(job_flag_t::NEGATE) ? !laststatus : laststatus);
    return st;
}

void internal_proc_t::mark_exited(proc_status_t status) {
    assert(!exited() && "Process is already exited");
    status_.store(status, std::memory_order_relaxed);
    exited_.store(true, std::memory_order_release);
    topic_monitor_t::principal().post(topic_t::internal_exit);
}

static void mark_job_complete(const job_t *j) {
    for (auto &p : j->processes) {
        p->completed = 1;
    }
}

void job_mark_process_as_failed(const std::shared_ptr<job_t> &job, const process_t *failed_proc) {
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

/// Set the status of \p proc to \p status.
static void handle_child_status(process_t *proc, proc_status_t status) {
    proc->status = status;
    if (status.stopped()) {
        proc->stopped = true;
    } else {
        proc->completed = true;
    }

    // If the child was killed by SIGINT or SIGQUIT, then treat it as if we received that signal.
    if (status.signal_exited()) {
        int sig = status.signal_code();
        if (sig == SIGINT || sig == SIGQUIT) {
            if (is_interactive_session) {
                // In an interactive session, tell the principal parser to skip all blocks we're
                // executing so control-C returns control to the user.
                parser_t::skip_all_blocks();
            } else {
                // Deliver the SIGINT or SIGQUIT signal to ourself since we're not interactive.
                struct sigaction act;
                sigemptyset(&act.sa_mask);
                act.sa_flags = 0;
                act.sa_handler = SIG_DFL;
                sigaction(sig, &act, 0);
                kill(getpid(), sig);
            }
        }
    }
}

process_t::process_t() = default;

void process_t::check_generations_before_launch() {
    gens_ = topic_monitor_t::principal().current_generations();
}

job_t::job_t(job_id_t jobid, io_chain_t bio, std::shared_ptr<job_t> parent)
    : block_io(std::move(bio)),
      parent_job(std::move(parent)),
      pgid(INVALID_PID),
      tmodes(),
      job_id(jobid),
      flags{} {}

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

/// A list of pids/pgids that have been disowned. They are kept around until either they exit or
/// we exit. Poll these from time-to-time to prevent zombie processes from happening (#5342).
static std::vector<pid_t> s_disowned_pids;

void add_disowned_pgid(pid_t pgid) {
    // NEVER add our own (or an invalid) pgid as they are not unique to only
    // one job, and may result in a deadlock if we attempt the wait.
    if (pgid != getpgrp() && pgid > 0) {
        // waitpid(2) is signalled to wait on a process group rather than a
        // process id by using the negative of its value.
        s_disowned_pids.push_back(pgid * -1);
    }
}

/// See if any reapable processes have exited, and mark them accordingly.
/// \param block_ok if no reapable processes have exited, block until one is (or until we receive a
/// signal).
static void process_mark_finished_children(bool block_ok) {
    ASSERT_IS_MAIN_THREAD();

    // Get the exit and signal generations of all reapable processes.
    // The exit generation tells us if we have an exit; the signal generation allows for detecting
    // SIGHUP and SIGINT.
    // Get the gen count of all reapable processes.
    topic_set_t reaptopics{};
    generation_list_t gens{};
    gens.fill(invalid_generation);
    for (const auto j: jobs()) {
        for (const auto &proc : j->processes) {
            if (auto mtopic = j->reap_topic_for_process(proc.get())) {
                topic_t topic = *mtopic;
                reaptopics.set(topic);
                gens[topic] = std::min(gens[topic], proc->gens_[topic]);

                reaptopics.set(topic_t::sighupint);
                gens[topic_t::sighupint] =
                    std::min(gens[topic_t::sighupint], proc->gens_[topic_t::sighupint]);
            }
        }
    }

    if (reaptopics.none()) {
        // No reapable processes, nothing to wait for.
        return;
    }

    // Now check for changes, optionally waiting.
    auto changed_topics = topic_monitor_t::principal().check(&gens, reaptopics, block_ok);
    if (changed_topics.none()) return;

    // We got some changes. Since we last checked we received SIGCHLD, and or HUP/INT.
    // Update the hup/int generations and reap any reapable processes.
    for (const auto &j : jobs()) {
        for (const auto &proc : j->processes) {
            if (auto mtopic = j->reap_topic_for_process(proc.get())) {
                // Update the signal hup/int gen.
                proc->gens_[topic_t::sighupint] = gens[topic_t::sighupint];

                if (proc->gens_[*mtopic] < gens[*mtopic]) {
                    // Potentially reapable. Update its gen count and try reaping it.
                    proc->gens_[*mtopic] = gens[*mtopic];
                    if (proc->internal_proc_) {
                        // Try reaping an internal process.
                        if (proc->internal_proc_->exited()) {
                            proc->status = proc->internal_proc_->get_status();
                            proc->completed = true;
                        }
                    } else if (proc->pid > 0) {
                        // Try reaping an external process.
                        int status = -1;
                        auto pid = waitpid(proc->pid, &status, WNOHANG | WUNTRACED);
                        if (pid > 0) {
                            assert(pid == proc->pid && "Unexpcted waitpid() return");
                            debug(4, "Reaped PID %d", pid);
                            handle_child_status(proc.get(), proc_status_t::from_waitpid(status));
                        }
                    } else {
                        assert(0 && "Don't know how to reap this process");
                    }
                }
            }
        }
    }

    // Poll disowned processes/process groups, but do nothing with the result. Only used to avoid
    // zombie processes. Entries have already been converted to negative for process groups.
    int status;
    s_disowned_pids.erase(std::remove_if(s_disowned_pids.begin(), s_disowned_pids.end(),
                [&status](pid_t pid) { return waitpid(pid, &status, WNOHANG) > 0; }),
            s_disowned_pids.end());
}

/// Given a command like "cat file", truncate it to a reasonable length.
static wcstring truncate_command(const wcstring &cmd) {
    const size_t max_len = 32;
    if (cmd.size() <= max_len) {
        // No truncation necessary.
        return cmd;
    }

    // Truncation required.
    const size_t ellipsis_length = std::wcslen(ellipsis_str);  // no need for wcwidth
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
static void print_job_status(const job_t *j, job_status_t status) {
    const wchar_t *msg = L"Job %d, '%ls' has ended";  // this is the most common status msg
    if (status == JOB_STOPPED) msg = L"Job %d, '%ls' has stopped";
    outputter_t outp;
    outp.writestr("\r");
    outp.writestr(format_string(_(msg), j->job_id, truncate_command(j->command()).c_str()));
    if (clr_eol) outp.term_puts(clr_eol, 1);
    outp.writestr(L"\n");
    fflush(stdout);
    outp.flush_to(STDOUT_FILENO);
}

void proc_fire_event(const wchar_t *msg, event_type_t type, pid_t pid, int status) {
    event_t event{type};
    event.desc.param1.pid = pid;

    event.arguments.push_back(msg);
    event.arguments.push_back(to_string(pid));
    event.arguments.push_back(to_string(status));
    event_fire(event);
}

static bool process_clean_after_marking(bool allow_interactive) {
    ASSERT_IS_MAIN_THREAD();
    bool found = false;

    // This function may fire an event handler, we do not want to call ourselves recursively (to
    // avoid infinite recursion).
    static std::atomic<bool> locked { false };
    if (locked.exchange(true, std::memory_order::memory_order_relaxed)) {
        return false;
    }

    // This may be invoked in an exit handler, after the TERM has been torn down
    // Don't try to print in that case (#3222)
    const bool interactive = allow_interactive && cur_term != NULL;

    // If we ever drop the `static bool locked` above, this should be changed to a local or
    // thread-local vector instead of a static vector. It is only static to reduce heap allocations.
    static std::vector<shared_ptr<job_t>> erase_list;
    const bool only_one_job = jobs().size() == 1;
    for (const auto &j : jobs()) {
        if (!j->is_constructed()) {
            continue;
        }
        // If we are reaping only jobs who do not need status messages sent to the console, do not
        // consider reaping jobs that need status messages.
        if ((!j->get_flag(job_flag_t::SKIP_NOTIFICATION)) && (!interactive) &&
            (!j->is_foreground())) {
            continue;
        }

        for (const process_ptr_t &p : j->processes) {
            if (!p->completed || !p->pid) {
                continue;
            }

            auto s = p->status;
            // Ignore signal SIGPIPE.We issue it ourselves to the pipe writer when the pipe reader
            // dies.
            if (!s.signal_exited() || s.signal_code() == SIGPIPE) {
                continue;
            }

            // Handle signals other than SIGPIPE.
            int proc_is_job = (p->is_first_in_job && p->is_last_in_job);
            if (proc_is_job) j->set_flag(job_flag_t::NOTIFIED, true);
            // Always report crashes.
            if (j->get_flag(job_flag_t::SKIP_NOTIFICATION) &&
                !contains(crashsignals, s.signal_code())) {
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
            if (s.signal_code() != SIGINT || !j->is_foreground()) {
                if (proc_is_job) {
                    // We want to report the job number, unless it's the only job, in which case
                    // we don't need to.
                    const wcstring job_number_desc =
                        only_one_job ? wcstring() : format_string(_(L"Job %d, "), j->job_id);
                    std::fwprintf(stdout, _(L"%ls: %ls\'%ls\' terminated by signal %ls (%ls)"),
                             program_name, job_number_desc.c_str(),
                             truncate_command(j->command()).c_str(), sig2wcs(s.signal_code()),
                             signal_get_desc(s.signal_code()));
                } else {
                    const wcstring job_number_desc =
                        only_one_job ? wcstring() : format_string(L"from job %d, ", j->job_id);
                    const wchar_t *fmt =
                        _(L"%ls: Process %d, \'%ls\' %ls\'%ls\' terminated by signal %ls (%ls)");
                    std::fwprintf(stdout, fmt, program_name, p->pid, p->argv0(), job_number_desc.c_str(),
                             truncate_command(j->command()).c_str(), sig2wcs(s.signal_code()),
                             signal_get_desc(s.signal_code()));
                }

                if (clr_eol) outputter_t::stdoutput().term_puts(clr_eol, 1);
                std::fwprintf(stdout, L"\n");
            }
            found = false;
            // clear status so it is not reported more than once
            p->status = proc_status_t::from_exit_code(0);
        }

        // If the process has been previously flagged for removal, add it to the erase list without
        // any further processing, but do not remove any jobs until their parent jobs have completed
        // processing.
        if (j->get_flag(job_flag_t::PENDING_REMOVAL) && j->job_chain_is_fully_constructed()) {
            erase_list.push_back(j);
        }
        // If all processes have completed, tell the user the job has completed and delete it from
        // the active job list.
        else if (j->is_completed()) {
            if (!j->is_foreground() && !j->get_flag(job_flag_t::NOTIFIED) &&
                !j->get_flag(job_flag_t::SKIP_NOTIFICATION)) {
                print_job_status(j.get(), JOB_ENDED);
                found = true;
            }

            erase_list.push_back(j);
        } else if (j->is_stopped() && !j->get_flag(job_flag_t::NOTIFIED)) {
            // Notify the user about newly stopped jobs.
            if (!j->get_flag(job_flag_t::SKIP_NOTIFICATION)) {
                print_job_status(j.get(), JOB_STOPPED);
                found = true;
            }
            j->set_flag(job_flag_t::NOTIFIED, true);
        }
    }

    if (!erase_list.empty()) {
        // The intersection of the two lists is typically O(m*n), but we know the order
        // of the entries in erase_list is the same as their matches in jobs(), so we can
        // use that to our advantage.
        auto to_erase = erase_list.begin();
        jobs().erase(std::remove_if(jobs().begin(), jobs().end(),
                    [&to_erase](const shared_ptr<job_t> &j) {
            if (to_erase != erase_list.end() && *to_erase == j) {
                ++to_erase;
                return true;
            }
            return false;
        }), jobs().end());

        if (should_debug(2)) {
            // Assertions prevent the application from continuing in case of invalid state. If we
            // did not remove all objects from the list, it's not bad enough to abort and die, but
            // leave this check here so that we can be alerted to the situtaion if running at a
            // higher debug level. Or just remove it. But I wouldn't want to be the person that has
            // to debug this without even this soft assertion in place when some C++ standard
            // library decides to make std::remove_if iterate backwards or in random order!
            assert(to_erase == erase_list.end()
                && "Not all jobs slated for erasure have been erased!");
        }
    }

    // Only now notify any listeners of the job exit, so that the contract isn't violated
    for (const auto &j : erase_list) {
        proc_fire_event(L"JOB_EXIT", event_type_t::job_exit, j->job_id, 0);
    }

    erase_list.clear();

    if (found) {
        fflush(stdout);
    }

    locked = false;
    return found;
}

bool job_reap(bool allow_interactive) {
    ASSERT_IS_MAIN_THREAD();
    bool found = false;

    process_mark_finished_children(false);

    // Preserve the exit status.
    auto saved_statuses = proc_get_last_statuses();

    found = process_clean_after_marking(allow_interactive);

    // Restore the exit status.
    proc_set_last_statuses(std::move(saved_statuses));

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

    std::swprintf(fn, FN_SIZE, L"/proc/%d/stat", p->pid);
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
    for (const auto &job : jobs()) {
        for (process_ptr_t &p : job->processes) {
            gettimeofday(&p->last_time, 0);
            p->last_jiffies = proc_get_jiffies(p.get());
        }
    }
}

#endif

// Return control of the terminal to a job's process group. restore_attrs is true if we are restoring
// a previously-stopped job, in which case we need to restore terminal attributes.
bool terminal_give_to_job(const job_t *j, bool restore_attrs) {
    if (j->pgid == 0) {
        debug(2, "terminal_give_to_job() returning early due to no process group");
        return true;
    }

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
                debug(1, _(L"Could not send job %d ('%ls') with pgid %d to foreground"), j->job_id,
                      j->command_wcstr(), j->pgid);
                wperror(L"tcsetpgrp");
                return false;
            }

            if (pgroup_terminated) {
                // All processes in the process group has exited.
                // Since we delay reaping any processes in a process group until all members of that
                // job/group have been started, the only way this can happen is if the very last
                // process in the group terminated and didn't need to access the terminal, otherwise
                // it would have hung waiting for terminal IO (SIGTTIN). We can safely ignore this.
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

            debug(1, _(L"Could not send job %d ('%ls') to foreground"), j->job_id,
                  j->preview().c_str());
            wperror(L"tcsetattr");
            return false;
        }
    }

    return true;
}

pid_t terminal_acquire_before_builtin(int job_pgid) {
    pid_t selfpgid = getpgrp();

    pid_t current_owner = tcgetpgrp(STDIN_FILENO);
    if (current_owner >= 0 && current_owner != selfpgid && current_owner == job_pgid) {
        if (tcsetpgrp(STDIN_FILENO, selfpgid) == 0) {
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

    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        if (errno == ENOTTY) redirect_tty_output();
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcsetpgrp");
        return false;
    }

    // Save jobs terminal modes.
    if (tcgetattr(STDIN_FILENO, &j->tmodes)) {
        if (errno == EIO) redirect_tty_output();
        debug(1, _(L"Could not return shell to foreground"));
        wperror(L"tcgetattr");
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

    return true;
}

void job_t::continue_job(bool reclaim_foreground_pgrp, bool send_sigcont) {
    // Put job first in the job list.
    promote();
    set_flag(job_flag_t::NOTIFIED, false);

    debug(4, L"%ls job %d, gid %d (%ls), %ls, %ls", send_sigcont ? L"Continue" : L"Start", job_id,
          pgid, command_wcstr(), is_completed() ? L"COMPLETED" : L"UNCOMPLETED",
          is_interactive ? L"INTERACTIVE" : L"NON-INTERACTIVE");

    // Make sure we retake control of the terminal before leaving this function.
    bool term_transferred = false;
    cleanup_t take_term_back([&]() {
        if (term_transferred && reclaim_foreground_pgrp) {
            terminal_return_from_job(this);
        }
    });

    if (!is_completed()) {
        if (get_flag(job_flag_t::TERMINAL) && is_foreground()) {
            // Put the job into the foreground and give it control of the terminal.
            // Hack: ensure that stdin is marked as blocking first (issue #176).
            make_fd_blocking(STDIN_FILENO);
            if (!terminal_give_to_job(this, send_sigcont)) {
                // This scenario has always returned without any error handling. Presumably that is
                // OK.
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
            // Wait for the status of our own job to change.
            while (!reader_exit_forced() && !is_stopped() && !is_completed()) {
                process_mark_finished_children(true);
            }
        }
    }

    if (is_foreground() && is_completed()) {
        // Set $status only if we are in the foreground and the last process in the job has
        // finished and is not a short-circuited builtin.
        auto &p = processes.back();
        if (p->status.normal_exited() || p->status.signal_exited()) {
            proc_set_last_statuses(get_statuses());
        }
    }
}

void proc_sanity_check() {
    const job_t *fg_job = NULL;

    for (const auto &j : jobs()) {
        if (!j->is_constructed()) continue;

        // More than one foreground job?
        if (j->is_foreground() && !(j->is_stopped() || j->is_completed())) {
            if (fg_job) {
                debug(0, _(L"More than one job in foreground: job 1: '%ls' job 2: '%ls'"),
                      fg_job->command_wcstr(), j->command_wcstr());
                sanity_lose();
            }
            fg_job = j.get();
        }

        for (const process_ptr_t &p : j->processes) {
            // Internal block nodes do not have argv - see issue #1545.
            bool null_ok = (p->type == process_type_t::block_node);
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

void proc_wait_any() {
    ASSERT_IS_MAIN_THREAD();
    process_mark_finished_children(true /* block_ok */);
    process_clean_after_marking(is_interactive);
}

void hup_background_jobs() {
    for (const auto &j : jobs()) {
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

static std::atomic<bool> s_is_within_fish_initialization{false};

void set_is_within_fish_initialization(bool flag) { s_is_within_fish_initialization.store(flag); }

bool is_within_fish_initialization() { return s_is_within_fish_initialization.load(); }
