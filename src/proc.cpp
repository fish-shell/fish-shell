// Utilities for keeping track of jobs, processes and subshells, as well as signal handling
// functions for tracking children. These functions do not themselves launch new processes, the exec
// library will call proc to create representations of the running jobs as needed.
//
// Some of the code in this file is based on code from the Glibc manual.
// IWYU pragma: no_include <__bit_reference>
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <wctype.h>

#include <atomic>
#include <cwchar>

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
#include "flog.h"
#include "global_safety.h"
#include "io.h"
#include "output.h"
#include "parse_tree.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "sanity.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

/// The signals that signify crashes to us.
static const int crashsignals[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGSYS};

static relaxed_atomic_t<session_interactivity_t> s_is_interactive_session{
    session_interactivity_t::not_interactive};
session_interactivity_t session_interactivity() { return s_is_interactive_session; }
void set_interactive_session(session_interactivity_t flag) { s_is_interactive_session = flag; }

static relaxed_atomic_bool_t s_is_login{false};
bool get_login() { return s_is_login; }
void mark_login() { s_is_login = true; }

static relaxed_atomic_bool_t s_no_exec{false};
bool no_exec() { return s_no_exec; }
void mark_no_exec() { s_no_exec = true; }

bool have_proc_stat() {
    // Check for /proc/self/stat to see if we are running with Linux-style procfs.
    static const bool s_result = (access("/proc/self/stat", R_OK) == 0);
    return s_result;
}

static relaxed_atomic_t<job_control_t> job_control_mode{job_control_t::interactive};

job_control_t get_job_control_mode() { return job_control_mode; }

void set_job_control_mode(job_control_t mode) { job_control_mode = mode; }

void proc_init() { signal_set_handlers_once(false); }

// Basic thread safe sorted vector of job IDs in use.
// This is deliberately leaked to avoid dtor ordering issues - see #6539.
static auto *const locked_consumed_job_ids = new owning_lock<std::vector<job_id_t>>();

job_id_t acquire_job_id() {
    auto consumed_job_ids = locked_consumed_job_ids->acquire();

    // The new job ID should be larger than the largest currently used ID (#6053).
    job_id_t jid = consumed_job_ids->empty() ? 1 : consumed_job_ids->back() + 1;
    consumed_job_ids->push_back(jid);
    return jid;
}

void release_job_id(job_id_t jid) {
    assert(jid > 0);
    auto consumed_job_ids = locked_consumed_job_ids->acquire();

    // Our job ID vector is sorted, but the number of jobs is typically 1 or 2 so a binary search
    // isn't worth it.
    auto where = std::find(consumed_job_ids->begin(), consumed_job_ids->end(), jid);
    assert(where != consumed_job_ids->end() && "JobID was not in use");
    consumed_job_ids->erase(where);
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

bool job_t::should_report_process_exits() const {
    // This implements the behavior of process exit events only being sent for jobs containing an
    // external process. Bizarrely the process exit event is for the pgroup leader which may be fish
    // itself.
    // TODO: rationalize this.
    // If we never got a pgid then we never launched the external process, so don't report it.
    if (this->pgid == INVALID_PID) {
        return false;
    }

    // Return whether we have an external process.
    for (const auto &p : this->processes) {
        if (p->type == process_type_t::external) {
            return true;
        }
    }
    return false;
}

bool job_t::job_chain_is_fully_constructed() const { return *root_constructed; }

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
    st.status = flags().negate ? !laststatus : laststatus;
    return st;
}

void internal_proc_t::mark_exited(proc_status_t status) {
    assert(!exited() && "Process is already exited");
    status_.store(status, std::memory_order_relaxed);
    exited_.store(true, std::memory_order_release);
    topic_monitor_t::principal().post(topic_t::internal_exit);
    FLOG(proc_internal_proc, L"Internal proc", internal_proc_id_, L"exited with status",
         status.status_value());
}

static int64_t next_proc_id() {
    static std::atomic<uint64_t> s_next{};
    return ++s_next;
}

internal_proc_t::internal_proc_t() : internal_proc_id_(next_proc_id()) {}

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
            if (session_interactivity() != session_interactivity_t::not_interactive) {
                // In an interactive session, tell the principal parser to skip all blocks we're
                // executing so control-C returns control to the user.
                parser_t::cancel_requested(sig);
            } else {
                // Deliver the SIGINT or SIGQUIT signal to ourself since we're not interactive.
                struct sigaction act;
                sigemptyset(&act.sa_mask);
                act.sa_flags = 0;
                act.sa_handler = SIG_DFL;
                sigaction(sig, &act, nullptr);
                kill(getpid(), sig);
            }
        }
    }
}

process_t::process_t() = default;

void process_t::check_generations_before_launch() {
    gens_ = topic_monitor_t::principal().current_generations();
}

job_t::job_t(job_id_t job_id, const properties_t &props, const job_lineage_t &lineage)
    : properties(props),
      job_id_(job_id),
      root_constructed(lineage.root_constructed ? lineage.root_constructed : this->constructed) {}

job_t::~job_t() {
    if (job_id_ != -1) release_job_id(job_id_);
}

void job_t::mark_constructed() {
    assert(!is_constructed() && "Job was already constructed");
    *constructed = true;
}

/// A list of pids/pgids that have been disowned. They are kept around until either they exit or
/// we exit. Poll these from time-to-time to prevent zombie processes from happening (#5342).
static owning_lock<std::vector<pid_t>> s_disowned_pids;

void add_disowned_pgid(pid_t pgid) {
    // NEVER add our own (or an invalid) pgid as they are not unique to only
    // one job, and may result in a deadlock if we attempt the wait.
    if (pgid != getpgrp() && pgid > 0) {
        // waitpid(2) is signalled to wait on a process group rather than a
        // process id by using the negative of its value.
        s_disowned_pids.acquire()->push_back(pgid * -1);
    }
}

// Reap any pids in our disowned list that have exited. This is used to avoid zombies.
static void reap_disowned_pids() {
    auto disowned_pids = s_disowned_pids.acquire();
    auto try_reap1 = [](pid_t pid) {
        int status;
        return waitpid(pid, &status, WNOHANG) > 0;
    };
    disowned_pids->erase(std::remove_if(disowned_pids->begin(), disowned_pids->end(), try_reap1),
                         disowned_pids->end());
}

/// See if any reapable processes have exited, and mark them accordingly.
/// \param block_ok if no reapable processes have exited, block until one is (or until we receive a
/// signal).
static void process_mark_finished_children(parser_t &parser, bool block_ok) {
    ASSERT_IS_MAIN_THREAD();

    // Get the exit and signal generations of all reapable processes.
    // The exit generation tells us if we have an exit; the signal generation allows for detecting
    // SIGHUP and SIGINT.
    // Get the gen count of all reapable processes.
    topic_set_t reaptopics{};
    generation_list_t gens{};
    gens.fill(invalid_generation);
    for (const auto &j : parser.jobs()) {
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
    for (const auto &j : parser.jobs()) {
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
                            handle_child_status(proc.get(), proc->internal_proc_->get_status());
                            FLOGF(proc_reap_internal,
                                  "Reaped internal process '%ls' (id %llu, status %d)",
                                  proc->argv0(), proc->internal_proc_->get_id(),
                                  proc->status.status_value());
                        }
                    } else if (proc->pid > 0) {
                        // Try reaping an external process.
                        int status = -1;
                        auto pid = waitpid(proc->pid, &status, WNOHANG | WUNTRACED);
                        if (pid > 0) {
                            assert(pid == proc->pid && "Unexpcted waitpid() return");
                            handle_child_status(proc.get(), proc_status_t::from_waitpid(status));
                            FLOGF(proc_reap_external,
                                  "Reaped external process '%ls' (pid %d, status %d)",
                                  proc->argv0(), pid, proc->status.status_value());
                        }
                    } else {
                        assert(0 && "Don't know how to reap this process");
                    }
                }
            }
        }
    }

    // Remove any zombies.
    reap_disowned_pids();
}

/// Given a command like "cat file", truncate it to a reasonable length.
static wcstring truncate_command(const wcstring &cmd) {
    const size_t max_len = 32;
    if (cmd.size() <= max_len) {
        // No truncation necessary.
        return cmd;
    }

    // Truncation required.
    const wchar_t *ellipsis_str = get_ellipsis_str();
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
    outp.writestr(format_string(_(msg), j->job_id(), truncate_command(j->command()).c_str()));
    if (clr_eol) outp.term_puts(clr_eol, 1);
    outp.writestr(L"\n");
    fflush(stdout);
    outp.flush_to(STDOUT_FILENO);
}

event_t proc_create_event(const wchar_t *msg, event_type_t type, pid_t pid, int status) {
    event_t event{type};
    event.desc.param1.pid = pid;

    event.arguments.push_back(msg);
    event.arguments.push_back(to_string(pid));
    event.arguments.push_back(to_string(status));
    return event;
}

/// Remove all disowned jobs whose job chain is fully constructed (that is, do not erase disowned
/// jobs that still have an in-flight parent job). Note we never print statuses for such jobs.
void remove_disowned_jobs(job_list_t &jobs) {
    auto iter = jobs.begin();
    while (iter != jobs.end()) {
        const auto &j = *iter;
        if (j->flags().disown_requested && j->job_chain_is_fully_constructed()) {
            iter = jobs.erase(iter);
        } else {
            ++iter;
        }
    }
}

/// Given a a process in a job, print the status message for the process as appropriate, and then
/// mark the status code so we don't print again. Populate any events into \p exit_events.
/// \return true if we printed a status message, false if not.
static bool try_clean_process_in_job(process_t *p, job_t *j, std::vector<event_t> *exit_events,
                                     bool only_one_job) {
    if (!p->completed || !p->pid) {
        return false;
    }

    auto s = p->status;

    // Add an exit event if the process did not come from a job handler.
    if (!j->from_event_handler()) {
        exit_events->push_back(proc_create_event(L"PROCESS_EXIT", event_type_t::exit, p->pid,
                                                 s.normal_exited() ? s.exit_code() : -1));
    }

    // Ignore SIGPIPE. We issue it ourselves to the pipe writer when the pipe reader dies.
    if (!s.signal_exited() || s.signal_code() == SIGPIPE) {
        return false;
    }

    int proc_is_job = (p->is_first_in_job && p->is_last_in_job);
    if (proc_is_job) j->mut_flags().notified = true;

    // Handle signals other than SIGPIPE.
    // Always report crashes.
    if (j->skip_notification() && !contains(crashsignals, s.signal_code())) {
        return false;
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
    bool printed = false;
    if (s.signal_code() != SIGINT || !j->is_foreground()) {
        if (proc_is_job) {
            // We want to report the job number, unless it's the only job, in which case
            // we don't need to.
            const wcstring job_number_desc =
                only_one_job ? wcstring() : format_string(_(L"Job %d, "), j->job_id());
            std::fwprintf(stdout, _(L"%ls: %ls\'%ls\' terminated by signal %ls (%ls)"),
                          program_name, job_number_desc.c_str(),
                          truncate_command(j->command()).c_str(), sig2wcs(s.signal_code()),
                          signal_get_desc(s.signal_code()));
        } else {
            const wcstring job_number_desc =
                only_one_job ? wcstring() : format_string(L"from job %d, ", j->job_id());
            const wchar_t *fmt =
                _(L"%ls: Process %d, \'%ls\' %ls\'%ls\' terminated by signal %ls (%ls)");
            std::fwprintf(stdout, fmt, program_name, p->pid, p->argv0(), job_number_desc.c_str(),
                          truncate_command(j->command()).c_str(), sig2wcs(s.signal_code()),
                          signal_get_desc(s.signal_code()));
        }

        if (clr_eol) outputter_t::stdoutput().term_puts(clr_eol, 1);
        std::fwprintf(stdout, L"\n");
        printed = true;
    }
    // Clear status so it is not reported more than once.
    // TODO: this seems like a clumsy way to ensure that.
    p->status = proc_status_t::from_exit_code(0);
    return printed;
}

/// \return whether this job wants a status message printed when it stops or completes.
static bool job_wants_message(const shared_ptr<job_t> &j) {
    // Did we already print a status message?
    if (j->flags().notified) return false;

    // Do we just skip notifications?
    if (j->skip_notification()) return false;

    // Are we foreground?
    // The idea here is to not print status messages for jobs that execute in the foreground (i.e.
    // without & and without being `bg`).
    if (j->is_foreground()) return false;

    return true;
}

/// Remove completed jobs from the job list, printing status messages as appropriate.
/// \return whether something was printed.
static bool process_clean_after_marking(parser_t &parser, bool allow_interactive) {
    ASSERT_IS_MAIN_THREAD();
    bool printed = false;

    // This function may fire an event handler, we do not want to call ourselves recursively (to
    // avoid infinite recursion).
    if (parser.libdata().is_cleaning_procs) {
        return false;
    }
    parser.libdata().is_cleaning_procs = true;
    const cleanup_t cleanup([&] { parser.libdata().is_cleaning_procs = false; });

    // This may be invoked in an exit handler, after the TERM has been torn down
    // Don't try to print in that case (#3222)
    const bool interactive = allow_interactive && cur_term != nullptr;

    // Remove all disowned jobs.
    remove_disowned_jobs(parser.jobs());

    // Accumulate exit events into a new list, which we fire after the list manipulation is
    // complete.
    std::vector<event_t> exit_events;

    // Print status messages for completed or stopped jobs.
    const bool only_one_job = parser.jobs().size() == 1;
    for (const auto &j : parser.jobs()) {
        // Skip unconstructed jobs.
        if (!j->is_constructed()) {
            continue;
        }

        // If we are not interactive, skip cleaning jobs that want to print an interactive message.
        if (!interactive && job_wants_message(j)) {
            continue;
        }

        // Clean processes within the job.
        // Note this may print the message on behalf of the job, affecting the result of
        // job_wants_message().
        for (process_ptr_t &p : j->processes) {
            if (try_clean_process_in_job(p.get(), j.get(), &exit_events, only_one_job)) {
                printed = true;
            }
        }

        // Print the message if we need to.
        if (job_wants_message(j) && (j->is_completed() || j->is_stopped())) {
            print_job_status(j.get(), j->is_completed() ? JOB_ENDED : JOB_STOPPED);
            j->mut_flags().notified = true;
            printed = true;
        }

        // Prepare events for completed jobs, except for jobs that themselves came from event
        // handlers.
        if (!j->from_event_handler() && j->is_completed()) {
            if (j->should_report_process_exits()) {
                exit_events.push_back(
                    proc_create_event(L"JOB_EXIT", event_type_t::exit, -j->pgid, 0));
            }
            exit_events.push_back(
                proc_create_event(L"JOB_EXIT", event_type_t::job_exit, j->job_id(), 0));
        }
    }

    // Remove completed jobs.
    // Do this before calling out to user code in the event handler below, to ensure an event
    // handler doesn't remove jobs on our behalf.
    auto is_complete = [](const shared_ptr<job_t> &j) { return j->is_completed(); };
    auto &jobs = parser.jobs();
    jobs.erase(std::remove_if(jobs.begin(), jobs.end(), is_complete), jobs.end());

    // Post pending exit events.
    for (const auto &evt : exit_events) {
        event_fire(parser, evt);
    }

    if (printed) {
        fflush(stdout);
    }

    return printed;
}

bool job_reap(parser_t &parser, bool allow_interactive) {
    ASSERT_IS_MAIN_THREAD();
    process_mark_finished_children(parser, false);

    // Preserve the exit status.
    auto saved_statuses = parser.get_last_statuses();

    bool printed = process_clean_after_marking(parser, allow_interactive);

    // Restore the exit status.
    parser.set_last_statuses(std::move(saved_statuses));

    return printed;
}

/// Get the CPU time for the specified process.
unsigned long proc_get_jiffies(process_t *p) {
    if (!have_proc_stat()) return 0;
    if (p->pid <= 0) return 0;

    char state;
    int pid, ppid, pgrp, session, tty_nr, tpgid, exit_signal, processor;
    long int cutime, cstime, priority, nice, placeholder, itrealvalue, rss;
    unsigned long int flags, minflt, cminflt, majflt, cmajflt, utime, stime, starttime, vsize, rlim,
        startcode, endcode, startstack, kstkesp, kstkeip, signal, blocked, sigignore, sigcatch,
        wchan, nswap, cnswap;
    char comm[1024];

    /// Maximum length of a /proc/[PID]/stat filename.
    constexpr size_t FN_SIZE = 256;
    char fn[FN_SIZE];
    std::snprintf(fn, FN_SIZE, "/proc/%d/stat", p->pid);
    // Don't use autoclose_fd here, we will fdopen() and then fclose() instead.
    int fd = open_cloexec(fn, O_RDONLY);
    if (fd < 0) return 0;

    // TODO: replace the use of fscanf() as it is brittle and should never be used.
    FILE *f = fdopen(fd, "r");
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
void proc_update_jiffies(parser_t &parser) {
    for (const auto &job : parser.jobs()) {
        for (process_ptr_t &p : job->processes) {
            gettimeofday(&p->last_time, nullptr);
            p->last_jiffies = proc_get_jiffies(p.get());
        }
    }
}

// Return control of the terminal to a job's process group. restore_attrs is true if we are
// restoring a previously-stopped job, in which case we need to restore terminal attributes.
int terminal_maybe_give_to_job(const job_t *j, bool continuing_from_stopped) {
    enum { notneeded = 0, success = 1, error = -1 };

    if (!j->should_claim_terminal()) {
        // The job doesn't want the terminal.
        return notneeded;
    }

    if (j->pgid == 0) {
        FLOG(proc_termowner, L"terminal_give_to_job() returning early due to no process group");
        return notneeded;
    }

    // If we are continuing, ensure that stdin is marked as blocking first (issue #176).
    if (continuing_from_stopped) {
        make_fd_blocking(STDIN_FILENO);
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
        FLOGF(proc_termowner, L"Process group %d already has control of terminal", j->pgid);
    } else {
        FLOGF(proc_termowner,
              L"Attempting to bring process group to foreground via tcsetpgrp for job->pgid %d",
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
            FLOGF(proc_termowner, L"tcsetpgrp failed: %d", errno);

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
                    FLOGF(proc_termowner, L"terminal_give_to_job(): EPERM.\n", j->pgid);
                    continue;
                }
            } else {
                if (errno == ENOTTY) {
                    redirect_tty_output();
                }
                FLOGF(warning, _(L"Could not send job %d ('%ls') with pgid %d to foreground"),
                      j->job_id(), j->command_wcstr(), j->pgid);
                wperror(L"tcsetpgrp");
                return error;
            }

            if (pgroup_terminated) {
                // All processes in the process group has exited.
                // Since we delay reaping any processes in a process group until all members of that
                // job/group have been started, the only way this can happen is if the very last
                // process in the group terminated and didn't need to access the terminal, otherwise
                // it would have hung waiting for terminal IO (SIGTTIN). We can safely ignore this.
                FLOGF(proc_termowner, L"tcsetpgrp called but process group %d has terminated.\n", j->pgid);
                return notneeded;
            }

            break;
        }
    }

    if (continuing_from_stopped) {
        auto result = tcsetattr(STDIN_FILENO, TCSADRAIN, &j->tmodes);
        if (result == -1) {
            // No need to test for EINTR and retry since we have blocked all signals
            if (errno == ENOTTY) {
                redirect_tty_output();
            }

            FLOGF(warning, _(L"Could not send job %d ('%ls') to foreground"), j->job_id(),
                  j->preview().c_str());
            wperror(L"tcsetattr");
            return error;
        }
    }

    return success;
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
static bool terminal_return_from_job(job_t *j, int restore_attrs) {
    errno = 0;
    if (j->pgid == 0) {
        FLOG(proc_pgroup, "terminal_return_from_job() returning early due to no process group");
        return true;
    }

    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        if (errno == ENOTTY) redirect_tty_output();
        FLOGF(warning, _(L"Could not return shell to foreground"));
        wperror(L"tcsetpgrp");
        return false;
    }

    // Save jobs terminal modes.
    if (tcgetattr(STDIN_FILENO, &j->tmodes)) {
        if (errno == EIO) redirect_tty_output();
        FLOGF(warning, _(L"Could not return shell to foreground"));
        wperror(L"tcgetattr");
        return false;
    }

    // Need to restore the terminal's attributes or `bind \cF fg` will put the
    // terminal into a broken state (until "enter" is pressed).
    // See: https://github.com/fish-shell/fish-shell/issues/2114
    if (restore_attrs) {
        if (tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_modes) == -1) {
            if (errno == EIO) redirect_tty_output();
            FLOGF(warning, _(L"Could not return shell to foreground"));
            wperror(L"tcsetattr");
            return false;
        }
    }

    return true;
}

void job_t::continue_job(parser_t &parser, bool reclaim_foreground_pgrp, bool send_sigcont) {
    // Put job first in the job list.
    parser.job_promote(this);
    mut_flags().notified = false;

    FLOGF(proc_job_run, L"%ls job %d, gid %d (%ls), %ls, %ls",
          send_sigcont ? L"Continue" : L"Start", job_id_, pgid, command_wcstr(),
          is_completed() ? L"COMPLETED" : L"UNCOMPLETED",
          parser.libdata().is_interactive ? L"INTERACTIVE" : L"NON-INTERACTIVE");

    // Make sure we retake control of the terminal before leaving this function.
    bool term_transferred = false;
    cleanup_t take_term_back([&]() {
        if (term_transferred && reclaim_foreground_pgrp) {
            // Only restore terminal attrs if we're continuing a job. See:
            // https://github.com/fish-shell/fish-shell/issues/121
            // https://github.com/fish-shell/fish-shell/issues/2114
            terminal_return_from_job(this, send_sigcont);
        }
    });

    if (!is_completed()) {
        int transfer = terminal_maybe_give_to_job(this, send_sigcont);
        if (transfer < 0) {
            // terminal_maybe_give_to_job prints an error.
            return;
        }
        term_transferred = (transfer > 0);

        // If both requested and necessary, send the job a continue signal.
        if (send_sigcont) {
            // This code used to check for JOB_CONTROL to decide between using killpg to signal all
            // processes in the group or iterating over each process in the group and sending the
            // signal individually. job_t::signal() does the same, but uses the shell's own pgroup
            // to make that distinction.
            if (!signal(SIGCONT)) {
                FLOGF(proc_pgroup, "Failed to send SIGCONT to any processes in pgroup %d!", pgid);
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
                process_mark_finished_children(parser, true);
            }
        }
    }

    if (is_foreground() && is_completed()) {
        // Set $status only if we are in the foreground and the last process in the job has
        // finished and is not a short-circuited builtin.
        auto &p = processes.back();
        if (p->status.normal_exited() || p->status.signal_exited()) {
            parser.set_last_statuses(get_statuses());
        }
    }
}

void proc_sanity_check(const parser_t &parser) {
    const job_t *fg_job = nullptr;

    for (const auto &j : parser.jobs()) {
        if (!j->is_constructed()) continue;

        // More than one foreground job?
        if (j->is_foreground() && !(j->is_stopped() || j->is_completed())) {
            if (fg_job) {
                FLOGF(error, _(L"More than one job in foreground: job 1: '%ls' job 2: '%ls'"),
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
                FLOGF(error, _(L"Job '%ls', process '%ls' has inconsistent state \'stopped\'=%d"),
                      j->command_wcstr(), p->argv0(), p->stopped);
                sanity_lose();
            }

            if ((p->completed & (~0x00000001)) != 0) {
                FLOGF(error, _(L"Job '%ls', process '%ls' has inconsistent state \'completed\'=%d"),
                      j->command_wcstr(), p->argv0(), p->completed);
                sanity_lose();
            }
        }
    }
}

void proc_wait_any(parser_t &parser) {
    ASSERT_IS_MAIN_THREAD();
    process_mark_finished_children(parser, true /* block_ok */);
    process_clean_after_marking(parser, parser.libdata().is_interactive);
}

void hup_background_jobs(const parser_t &parser) {
    // TODO: we should probably hup all jobs across all parsers here.
    for (const auto &j : parser.jobs()) {
        // Make sure we don't try to SIGHUP the calling builtin
        if (j->pgid == INVALID_PID || !j->wants_job_control()) {
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
