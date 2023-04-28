// Prototypes for utilities for keeping track of jobs, processes and subshells, as well as signal
// handling functions for tracking children. These functions do not themselves launch new processes,
// the exec library will call proc to create representations of the running jobs as needed.
#ifndef FISH_PROC_H
#define FISH_PROC_H
#include "config.h"  // IWYU pragma: keep

#include <sys/time.h>  // IWYU pragma: keep
#include <sys/wait.h>  // IWYU pragma: keep

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "common.h"
#include "cxx.h"
#include "maybe.h"
#include "parse_tree.h"
#include "parser.h"
#include "redirection.h"
#include "topic_monitor.h"

#if INCLUDE_RUST_HEADERS
#include "proc.rs.h"
#else
struct JobRefFfi;
struct JobGroupRefFfi;
#endif

#if 0
struct statuses_t;

/// Types of processes.
enum class process_type_t : uint8_t {
    /// A regular external command.
    external,
    /// A builtin command.
    builtin,
    /// A shellscript function.
    function,
    /// A block of commands, represented as a node.
    block_node,
    /// The exec builtin.
    exec,
};

enum class job_control_t : uint8_t {
    all,
    interactive,
    none,
};

/// A number of clock ticks.
using clock_ticks_t = uint64_t;

/// \return clock ticks in seconds, or 0 on failure.
/// This uses sysconf(_SC_CLK_TCK) to convert to seconds.
double clock_ticks_to_seconds(clock_ticks_t ticks);

struct job_group_t;
using job_group_ref_t = std::shared_ptr<job_group_t>;

/// A proc_status_t is a value type that encapsulates logic around exited vs stopped vs signaled,
/// etc.
class proc_status_t {
    int status_{};

    /// If set, there is no actual status to report, e.g. background or variable assignment.
    bool empty_{};

    explicit proc_status_t(int status) : status_(status), empty_(false) {}

    proc_status_t(int status, bool empty) : status_(status), empty_(empty) {}

    /// Encode a return value \p ret and signal \p sig into a status value like waitpid() does.
    static constexpr int w_exitcode(int ret, int sig) {
#ifdef W_EXITCODE
        return W_EXITCODE(ret, sig);
#elif HAVE_WAITSTATUS_SIGNAL_RET
        // It's encoded signal and then status
        // The return status is in the lower byte.
        return ((sig) << 8 | (ret));
#else
        // The status is encoded in the upper byte.
        return ((ret) << 8 | (sig));
#endif
    }

   public:
    proc_status_t() = default;

    /// Construct from a status returned from a waitpid call.
    static proc_status_t from_waitpid(int status) { return proc_status_t(status); }

    /// Construct directly from an exit code.
    static proc_status_t from_exit_code(int ret) {
        assert(ret >= 0 &&
               "trying to create proc_status_t from failed wait{,id,pid}() call"
               " or invalid builtin exit code!");

        // Some paranoia.
        constexpr int zerocode = w_exitcode(0, 0);
        static_assert(WIFEXITED(zerocode), "Synthetic exit status not reported as exited");

        assert(ret < 256);
        return proc_status_t(w_exitcode(ret, 0 /* sig */));
    }

    /// Construct directly from a signal.
    static proc_status_t from_signal(int sig) {
        return proc_status_t(w_exitcode(0 /* ret */, sig));
    }

    /// Construct an empty status_t (e.g. `set foo bar`).
    static proc_status_t empty() {
        bool empty = true;
        return proc_status_t(0, empty);
    }

    /// \return if we are stopped (as in SIGSTOP).
    bool stopped() const { return WIFSTOPPED(status_); }

    /// \return if we are continued (as in SIGCONT).
    bool continued() const { return WIFCONTINUED(status_); }

    /// \return if we exited normally (not a signal).
    bool normal_exited() const { return WIFEXITED(status_); }

    /// \return if we exited because of a signal.
    bool signal_exited() const { return WIFSIGNALED(status_); }

    /// \return the signal code, given that we signal exited.
    int signal_code() const {
        assert(signal_exited() && "Process is not signal exited");
        return WTERMSIG(status_);
    }

    /// \return the exit code, given that we normal exited.
    int exit_code() const {
        assert(normal_exited() && "Process is not normal exited");
        return WEXITSTATUS(status_);
    }

    /// \return if this status represents success.
    bool is_success() const { return normal_exited() && exit_code() == EXIT_SUCCESS; }

    /// \return if this status is empty.
    bool is_empty() const { return empty_; }

    /// \return the value appropriate to populate $status.
    int status_value() const {
        if (signal_exited()) {
            return 128 + signal_code();
        } else if (normal_exited()) {
            return exit_code();
        } else {
            DIE("Process is not exited");
        }
    }
};

/// A structure representing a "process" internal to fish. This is backed by a pthread instead of a
/// separate process.
class internal_proc_t {
    /// An identifier for internal processes.
    /// This is used for logging purposes only.
    const uint64_t internal_proc_id_;

    /// Whether the process has exited.
    std::atomic<bool> exited_{};

    /// If the process has exited, its status code.
    std::atomic<proc_status_t> status_{};

   public:
    /// \return if this process has exited.
    bool exited() const { return exited_.load(std::memory_order_acquire); }

    /// Mark this process as exited, with the given status.
    void mark_exited(proc_status_t status);

    proc_status_t get_status() const {
        assert(exited() && "Process is not exited");
        return status_.load(std::memory_order_relaxed);
    }

    uint64_t get_id() const { return internal_proc_id_; }

    internal_proc_t();
};

/// 0 should not be used; although it is not a valid PGID in userspace,
///   the Linux kernel will use it for kernel processes.
/// -1 should not be used; it is a possible return value of the getpgid()
///   function
enum { INVALID_PID = -2 };

// Allows transferring the tty to a job group, while it runs.
class tty_transfer_t : nonmovable_t, noncopyable_t {
   public:
    tty_transfer_t() = default;

    /// Transfer to the given job group, if it wants to own the terminal.
    void to_job_group(const job_group_ref_t &jg);

    /// Reclaim the tty if we transferred it.
    void reclaim();

    /// Save the current tty modes into the owning job group, if we are transferred.
    void save_tty_modes();

    /// The destructor will assert if reclaim() has not been called.
    ~tty_transfer_t();

   private:
    // Try transferring the tty to the given job group.
    // \return true if we should reclaim it.
    static bool try_transfer(const job_group_ref_t &jg);

    // The job group which owns the tty, or empty if none.
    job_group_ref_t owner_;
};

/// A structure representing a single fish process. Contains variables for tracking process state
/// and the process argument list. Actually, a fish process can be either a regular external
/// process, an internal builtin which may or may not spawn a fake IO process during execution, a
/// shellscript function or a block of commands to be evaluated by calling eval. Lastly, this
/// process can be the result of an exec command. The role of this process_t is determined by the
/// type field, which can be one of process_type_t::external, process_type_t::builtin,
/// process_type_t::function, process_type_t::exec.
///
/// The process_t contains information on how the process should be started, such as command name
/// and arguments, as well as runtime information on the status of the actual physical process which
/// represents it. Shellscript functions, builtins and blocks of code may all need to spawn an
/// external process that handles the piping and redirecting of IO for them.
///
/// If the process is of type process_type_t::external or process_type_t::exec, argv is the argument
/// array and actual_cmd is the absolute path of the command to execute.
///
/// If the process is of type process_type_t::builtin, argv is the argument vector, and argv[0] is
/// the name of the builtin command.
///
/// If the process is of type process_type_t::function, argv is the argument vector, and argv[0] is
/// the name of the shellscript function.
class process_t {
   public:
    process_t();

    /// Note whether we are the first and/or last in the job
    bool is_first_in_job{false};
    bool is_last_in_job{false};

    /// Type of process.
    process_type_t type{process_type_t::external};

    /// For internal block processes only, the node of the statement.
    /// This is always either block, ifs, or switchs, never boolean or decorated.
    rust::Box<ParsedSourceRefFFI> block_node_source;
    const ast::statement_t *internal_block_node{};

    struct concrete_assignment {
        wcstring variable_name;
        std::vector<wcstring> values;
    };
    /// The expanded variable assignments for this process, as specified by the `a=b cmd` syntax.
    std::vector<concrete_assignment> variable_assignments;

    /// Sets argv.
    void set_argv(std::vector<wcstring> argv) { argv_ = std::move(argv); }

    /// Returns argv.
    const std::vector<wcstring> &argv() { return argv_; }

    /// Returns argv[0], or nullptr.
    const wchar_t *argv0() const { return argv_.empty() ? nullptr : argv_.front().c_str(); }

    /// Redirection list getter and setter.
    const redirection_spec_list_t &redirection_specs() const { return *proc_redirection_specs_; }

    void set_redirection_specs(rust::Box<redirection_spec_list_t> specs) {
        this->proc_redirection_specs_ = std::move(specs);
    }

    /// Store the current topic generations. That is, right before the process is launched, record
    /// the generations of all topics; then we can tell which generation values have changed after
    /// launch. This helps us avoid spurious waitpid calls.
    void check_generations_before_launch();

    /// Mark that this process was part of a pipeline which was aborted.
    /// The process was never successfully launched; give it a status of EXIT_FAILURE.
    void mark_aborted_before_launch();

    /// \return whether this process type is internal (block, function, or builtin).
    bool is_internal() const;

    /// \return the wait handle for the process, if it exists.
    rust::Box<WaitHandleRefFFI> *get_wait_handle_ffi() const;

    /// Create a wait handle for the process.
    /// As a process does not know its job id, we pass it in.
    /// Note this will return null if the process is not waitable (has no pid).
    rust::Box<WaitHandleRefFFI> *make_wait_handle_ffi(internal_job_id_t jid);

    /// Variants of get and make that return void*, to satisfy autocxx.
    void *get_wait_handle_void() const;
    void *make_wait_handle_void(internal_job_id_t jid);

    /// Actual command to pass to exec in case of process_type_t::external or process_type_t::exec.
    wcstring actual_cmd;

    /// Generation counts for reaping.
    generation_list_t gens_{};

    /// Process ID
    pid_t pid{0};

    /// If we are an "internal process," that process.
    std::shared_ptr<internal_proc_t> internal_proc_{};

    /// File descriptor that pipe output should bind to.
    int pipe_write_fd{0};

    /// True if process has completed.
    bool completed{false};

    /// True if process has stopped.
    bool stopped{false};

    /// If set, this process is (or will become) the pgroup leader.
    /// This is only meaningful for external processes.
    bool leads_pgrp{false};

    /// Whether we have generated a proc_exit event.
    bool posted_proc_exit{false};

    /// Reported status value.
    proc_status_t status{};

    /// Last time of cpu time check, in seconds (per timef).
    timepoint_t last_time{0};

    /// Number of jiffies spent in process at last cpu time check.
    clock_ticks_t last_jiffies{0};

    process_t(process_t &&) = delete;
    process_t &operator=(process_t &&) = delete;
    process_t(const process_t &) = delete;
    process_t &operator=(const process_t &) = delete;

   private:
    std::vector<wcstring> argv_;
    rust::Box<redirection_spec_list_t> proc_redirection_specs_;

    // The wait handle. This is constructed lazily, and cached.
    // This may be null.
    std::unique_ptr<rust::Box<WaitHandleRefFFI>> wait_handle_;
};

using process_ptr_t = std::unique_ptr<process_t>;
using process_list_t = std::vector<process_ptr_t>;

struct RustFFIProcList {
    process_ptr_t *procs;
    size_t count;
};

/// A struct representing a job. A job is a pipeline of one or more processes.
class job_t : noncopyable_t {
   public:
    /// A set of jobs properties. These are immutable: they do not change for the lifetime of the
    /// job.
    struct properties_t {
        /// Whether the specified job is a part of a subshell, event handler or some other form of
        /// special job that should not be reported.
        bool skip_notification{};

        /// Whether the job had the background ampersand when constructed, e.g. /bin/echo foo &
        /// Note that a job may move between foreground and background; this just describes what the
        /// initial state should be.
        bool initial_background{};

        /// Whether the job has the 'time' prefix and so we should print timing for this job.
        bool wants_timing{};

        /// Whether this job was created as part of an event handler.
        bool from_event_handler{};
    };

   private:
    /// Set of immutable job properties.
    const properties_t properties;

    /// The original command which led to the creation of this job. It is used for displaying
    /// messages about job status on the terminal.
    const wcstring command_str;

   public:
    job_t(const properties_t &props, wcstring command_str);
    ~job_t();

    /// Autocxx needs to see this.
    job_t(const job_t &) = delete;

    /// Returns the command as a wchar_t *. */
    const wchar_t *command_wcstr() const { return command_str.c_str(); }

    /// Returns the command.
    const wcstring &command() const { return command_str; }

    /// \return whether it is OK to reap a given process. Sometimes we want to defer reaping a
    /// process if it is the group leader and the job is not yet constructed, because then we might
    /// also reap the process group and then we cannot add new processes to the group.
    bool can_reap(const process_ptr_t &p) const {
        if (p->completed) {
            // Can't reap twice.
            return false;
        } else if (p->pid && !is_constructed() && this->get_pgid() == maybe_t<pid_t>{p->pid}) {
            // p is the the group leader in an under-construction job.
            return false;
        } else {
            return true;
        }
    }

    /// Returns a truncated version of the job string. Used when a message has already been emitted
    /// containing the full job string and job id, but using the job id alone would be confusing
    /// due to reuse of freed job ids. Prevents overloading the debug comments with the full,
    /// untruncated job string when we don't care what the job is, only which of the currently
    /// running jobs it is.
    wcstring preview() const {
        if (processes.empty()) return L"";
        // Note argv0 may be empty in e.g. a block process.
        const wchar_t *argv0 = processes.front()->argv0();
        wcstring result = argv0 ? argv0 : L"null";
        return result + L" ...";
    }

    /// All the processes in this job.
    process_list_t processes;

    // The group containing this job.
    // This is never null and not changed after construction.
    job_group_ref_t group{};

    /// \return our pgid, or none if we don't have one, or are internal to fish
    /// This never returns fish's own pgroup.
    maybe_t<pid_t> get_pgid() const;

    /// \return the pid of the last external process in the job.
    /// This may be none if the job consists of just internal fish functions or builtins.
    /// This will never be fish's own pid.
    maybe_t<pid_t> get_last_pid() const;

    /// The id of this job.
    /// This is user-visible, is recycled, and may be -1.
    job_id_t job_id() const;

    /// A non-user-visible, never-recycled job ID.
    const internal_job_id_t internal_job_id;

    /// Getter to enable ffi.
    internal_job_id_t get_internal_job_id() const { return internal_job_id; }

    /// Flags associated with the job.
    struct flags_t {
        /// Whether the specified job is completely constructed: every process in the job has been
        /// forked, etc.
        bool constructed{false};

        /// Whether the user has been notified that this job is stopped (if it is).
        bool notified_of_stop{false};

        /// Whether the exit status should be negated. This flag can only be set by the not builtin.
        /// Two "not" prefixes on a single job cancel each other out.
        bool negate{false};

        /// This job is disowned, and should be removed from the active jobs list.
        bool disown_requested{false};

        // Indicates that we are the "group root." Any other jobs using this tree are nested.
        bool is_group_root{false};

    } job_flags{};

    /// Access the job flags.
    const flags_t &flags() const { return job_flags; }

    /// Access mutable job flags.
    flags_t &mut_flags() { return job_flags; }

    // \return whether we should print timing information.
    bool wants_timing() const { return properties.wants_timing; }

    /// \return if we want job control.
    bool wants_job_control() const;

    /// \return whether this job is initially going to run in the background, because & was
    /// specified.
    bool is_initially_background() const { return properties.initial_background; }

    /// Mark this job as constructed. The job must not have previously been marked as constructed.
    void mark_constructed();

    /// \return whether we have internal or external procs, respectively.
    /// Internal procs are builtins, blocks, and functions.
    /// External procs include exec and external.
    bool has_external_proc() const;

    /// \return whether this job, when run, will want a job ID.
    /// Jobs that are only a single internal block do not get a job ID.
    bool wants_job_id() const;

    // Helper functions to check presence of flags on instances of jobs
    /// The job has been fully constructed, i.e. all its member processes have been launched
    bool is_constructed() const { return flags().constructed; }
    /// The job is complete, i.e. all its member processes have been reaped
    bool is_completed() const;
    /// The job is in a stopped state
    bool is_stopped() const;
    /// The job is OK to be externally visible, e.g. to the user via `jobs`
    bool is_visible() const {
        return !is_completed() && is_constructed() && !flags().disown_requested;
    }
    bool skip_notification() const { return properties.skip_notification; }
    bool from_event_handler() const { return properties.from_event_handler; }

    /// \return whether this job's group is in the foreground.
    bool is_foreground() const;

    /// \return whether we should post job_exit events.
    bool posts_job_exit_events() const;

    /// Run ourselves. Returning once we complete or stop.
    void continue_job(parser_t &parser);

    /// Prepare to resume a stopped job by sending SIGCONT and clearing the stopped flag.
    /// \return true on success, false if we failed to send the signal.
    bool resume();

    /// Send the specified signal to all processes in this job.
    /// \return true on success, false on failure.
    bool signal(int signal);

    /// \returns the statuses for this job.
    maybe_t<statuses_t> get_statuses() const;

    /// \returns the list of processes.
    const process_list_t &get_processes() const;

    /// autocxx junk.
    RustFFIProcList ffi_processes() const;

    /// autocxx junk.
    const job_group_t &ffi_group() const;

    /// autocxx junk.
    /// The const is a lie and is only necessary since at the moment cxx's SharedPtr doesn't support
    /// getting a mutable reference.
    bool ffi_resume() const;
};
using job_ref_t = std::shared_ptr<job_t>;

// Helper junk for autocxx.
struct RustFFIJobList {
    job_ref_t *jobs;
    size_t count;
};

/// Whether this shell is attached to a tty.
bool is_interactive_session();
void set_interactive_session(bool flag);

/// Whether we are a login shell.
bool get_login();
void mark_login();

/// If this flag is set, fish will never fork or run execve. It is used to put fish into a syntax
/// verifier mode where fish tries to validate the syntax of a file but doesn't actually do
/// anything.
bool no_exec();
void mark_no_exec();

// List of jobs.
using job_list_t = std::vector<job_ref_t>;

/// The current job control mode.
///
/// Must be one of job_control_t::all, job_control_t::interactive and job_control_t::none.
job_control_t get_job_control_mode();
void set_job_control_mode(job_control_t mode);

/// Notify the user about stopped or terminated jobs, and delete completed jobs from the job list.
/// If \p interactive is set, allow removing interactive jobs; otherwise skip them.
/// \return whether text was printed to stdout.
bool job_reap(parser_t &parser, bool interactive);

/// \return the list of background jobs which we should warn the user about, if the user attempts to
/// exit. An empty result (common) means no such jobs.
job_list_t jobs_requiring_warning_on_exit(const parser_t &parser);

/// Print the exit warning for the given jobs, which should have been obtained via
/// jobs_requiring_warning_on_exit().
void print_exit_warning_for_jobs(const job_list_t &jobs);

/// Use the procfs filesystem to look up how many jiffies of cpu time was used by a given pid. This
/// function is only available on systems with the procfs file entry 'stat', i.e. Linux.
clock_ticks_t proc_get_jiffies(pid_t inpid);

/// Update process time usage for all processes by calling the proc_get_jiffies function for every
/// process of every job.
void proc_update_jiffies(parser_t &parser);

/// Initializations.
void proc_init();

/// Wait for any process finishing, or receipt of a signal.
void proc_wait_any(parser_t &parser);

/// Send SIGHUP to the list \p jobs, excepting those which are in fish's pgroup.
void hup_jobs(const job_list_t &jobs);

/// Add a job to the list of PIDs/PGIDs we wait on even though they are not associated with any
/// jobs. Used to avoid zombie processes after disown.
void add_disowned_job(const job_t *j);

bool have_proc_stat();

#endif
#endif
