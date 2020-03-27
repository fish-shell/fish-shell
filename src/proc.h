// Prototypes for utilities for keeping track of jobs, processes and subshells, as well as signal
// handling functions for tracking children. These functions do not themselves launch new processes,
// the exec library will call proc to create representations of the running jobs as needed.
#ifndef FISH_PROC_H
#define FISH_PROC_H
#include "config.h"  // IWYU pragma: keep

#include <signal.h>
#include <stddef.h>
#include <sys/time.h>  // IWYU pragma: keep
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <deque>
#include <memory>
#include <vector>

#include "common.h"
#include "event.h"
#include "global_safety.h"
#include "io.h"
#include "parse_tree.h"
#include "tnode.h"
#include "topic_monitor.h"

/// Types of processes.
enum class process_type_t {
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

enum class job_control_t {
    all,
    interactive,
    none,
};

/// A proc_status_t is a value type that encapsulates logic around exited vs stopped vs signaled,
/// etc.
class proc_status_t {
    int status_{};

    explicit proc_status_t(int status) : status_(status) {}

    /// Encode a return value \p ret and signal \p sig into a status value like waitpid() does.
    static constexpr int w_exitcode(int ret, int sig) {
#ifdef W_EXITCODE
        return W_EXITCODE(ret, sig);
#else
        return ((ret) << 8 | (sig));
#endif
    }

   public:
    proc_status_t() = default;

    /// Construct from a status returned from a waitpid call.
    static proc_status_t from_waitpid(int status) { return proc_status_t(status); }

    /// Construct directly from an exit code.
    static proc_status_t from_exit_code(int ret) {
        // Some paranoia.
        constexpr int zerocode = w_exitcode(0, 0);
        static_assert(WIFEXITED(zerocode), "Synthetic exit status not reported as exited");
        return proc_status_t(w_exitcode(ret, 0 /* sig */));
    }

    /// Construct directly from a signal.
    static proc_status_t from_signal(int sig) {
        return proc_status_t(w_exitcode(0 /* ret */, sig));
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
class parser_t;
class process_t {
   private:
    null_terminated_array_t<wchar_t> argv_array;

    redirection_spec_list_t proc_redirection_specs;

    // No copying.
    process_t(const process_t &rhs) = delete;
    void operator=(const process_t &rhs) = delete;

   public:
    process_t();

    /// Note whether we are the first and/or last in the job
    bool is_first_in_job{false};
    bool is_last_in_job{false};

    /// Type of process.
    process_type_t type{process_type_t::external};

    /// For internal block processes only, the node offset of the statement.
    /// This is always either block, ifs, or switchs, never boolean or decorated.
    parsed_source_ref_t block_node_source{};
    tnode_t<grammar::statement> internal_block_node{};

    struct concrete_assignment {
        wcstring variable_name;
        wcstring_list_t values;
    };
    /// The expanded variable assignments for this process, as specified by the `a=b cmd` syntax.
    std::vector<concrete_assignment> variable_assignments;

    /// Sets argv.
    void set_argv(const wcstring_list_t &argv) { argv_array.set(argv); }

    /// Returns argv.
    wchar_t **get_argv() { return argv_array.get(); }
    const wchar_t *const *get_argv() const { return argv_array.get(); }
    const null_terminated_array_t<wchar_t> &get_argv_array() const { return argv_array; }

    /// Returns argv[idx].
    const wchar_t *argv(size_t idx) const {
        const wchar_t *const *argv = argv_array.get();
        assert(argv != nullptr);
        return argv[idx];
    }

    /// Returns argv[0], or NULL.
    const wchar_t *argv0() const {
        const wchar_t *const *argv = argv_array.get();
        return argv ? argv[0] : nullptr;
    }

    /// Redirection list getter and setter.
    const redirection_spec_list_t &redirection_specs() const { return proc_redirection_specs; }
    void set_redirection_specs(redirection_spec_list_t specs) {
        this->proc_redirection_specs = std::move(specs);
    }

    /// Store the current topic generations. That is, right before the process is launched, record
    /// the generations of all topics; then we can tell which generation values have changed after
    /// launch. This helps us avoid spurious waitpid calls.
    void check_generations_before_launch();

    /// \return whether this process type is internal (block, function, or builtin).
    bool is_internal() const;

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
    /// Reported status value.
    proc_status_t status{};
    /// Last time of cpu time check.
    struct timeval last_time {};
    /// Number of jiffies spent in process at last cpu time check.
    unsigned long last_jiffies{0};
};

typedef std::unique_ptr<process_t> process_ptr_t;
typedef std::vector<process_ptr_t> process_list_t;

/// The non user-visible, never-recycled job ID.
/// Every job has a unique positive value for this.
using internal_job_id_t = uint64_t;

/// The user-visible, optional, recycled job ID.
using job_id_t = int;
job_id_t acquire_job_id(void);
void release_job_id(job_id_t jid);

/// Information about where a job comes from.
/// This should be safe to copy across threads; in particular that means this cannot contain a
/// job_t. It is also important that job_t not contain this: because it stores block IO, it will
/// extend the life of the IO which may prevent pipes from closing in a timely manner. See #6397.
struct job_lineage_t {
    /// The pgid of the parental job.
    /// If our job is "nested" as part of a function or block execution, and that function or block
    /// is part of a pipeline, then this may be set.
    maybe_t<pid_t> parent_pgid{};

    /// Whether job control is on for the root.
    /// This is set if our job is nested as part of a function or block execution.
    bool root_has_job_control{false};

    /// The IO chain associated with any block containing this job.
    /// For example, in `begin; foo ; end < file.txt` this would have the 'file.txt' IO.
    io_chain_t block_io{};

    /// A shared pointer indicating that the entire tree of jobs is safe to disown.
    /// This is set to true by the "root" job after it is constructed.
    std::shared_ptr<relaxed_atomic_bool_t> root_constructed{};
};

/// A job has a mode which describes how its pgroup is assigned (before the value is known).
/// This is a constant property of the job.
enum class pgroup_provenance_t {
    /// The job has no pgroup assignment. This is used for e.g. a simple function invocation with no
    /// pipeline.
    unassigned,

    /// The job's pgroup is fish's pgroup. This is used when fish needs to read from the terminal,
    /// or if job control is disabled.
    fish_itself,

    /// The job's pgroup will come from its first external process.
    first_external_proc,

    /// The job's pgroup will come from its lineage. This is used for jobs that are run nested.
    lineage,
};

/// A struct representing a job. A job is a pipeline of one or more processes.
class job_t {
   public:
    /// A set of jobs properties. These are immutable: they do not change for the lifetime of the
    /// job.
    struct properties_t {
        /// Whether the specified job is a part of a subshell, event handler or some other form of
        /// special job that should not be reported.
        bool skip_notification{};

        /// Whether the job wants to own the terminal when in the foreground.
        bool wants_terminal{};

        /// Whether this job was created as part of an event handler.
        bool from_event_handler{};

        /// Whether the job is under job control, i.e. has its own pgrp.
        bool job_control{};
    };

   private:
    /// Set of immutable job properties.
    const properties_t properties;

    /// The original command which led to the creation of this job. It is used for displaying
    /// messages about job status on the terminal.
    wcstring command_str;

    /// The job_id for this job.
    job_id_t job_id_;

    // No copying.
    job_t(const job_t &rhs) = delete;
    void operator=(const job_t &) = delete;

   public:
    job_t(job_id_t job_id, const properties_t &props, const job_lineage_t &lineage);
    ~job_t();

    /// Returns the command as a wchar_t *. */
    const wchar_t *command_wcstr() const { return command_str.c_str(); }

    /// Returns the command.
    const wcstring &command() const { return command_str; }

    /// Sets the command.
    void set_command(wcstring cmd) { command_str = std::move(cmd); }

    /// \return whether it is OK to reap a given process. Sometimes we want to defer reaping a
    /// process if it is the group leader and the job is not yet constructed, because then we might
    /// also reap the process group and then we cannot add new processes to the group.
    bool can_reap(const process_t *p) const {
        // Internal processes can always be reaped.
        if (p->internal_proc_) {
            return true;
        } else if (p->pid <= 0) {
            // Can't reap without a pid.
            return false;
        } else if (!is_constructed() && pgid > 0 && p->pid == pgid) {
            // p is the the group leader in an under-construction job.
            return false;
        } else {
            return true;
        }
    }

    /// \returns the reap topic for a process, which describes the manner in which we are reaped. A
    /// none returns means don't reap, or perhaps defer reaping.
    maybe_t<topic_t> reap_topic_for_process(const process_t *p) const {
        if (p->completed || !can_reap(p)) return none();
        return p->internal_proc_ ? topic_t::internal_exit : topic_t::sigchld;
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

    /// Process group ID for the process group that this job is running in.
    /// Set to a nonexistent, non-return-value of getpgid() integer by the constructor
    pid_t pgid{INVALID_PID};

    /// How the above pgroup is assigned. This should be set at construction and not modified after.
    pgroup_provenance_t pgroup_provenance{};

    /// The id of this job.
    /// This is user-visible, is recycled, and may be -1.
    job_id_t job_id() const { return job_id_; }

    /// A non-user-visible, never-recycled job ID.
    const internal_job_id_t internal_job_id;

    /// Mark this job as internal. Internal jobs' job_ids are removed from the
    /// list of jobs so that, among other things, they don't take a job_id
    /// entry.
    void mark_internal() {
        release_job_id(job_id_);
        job_id_ = -1;
    }

    /// The saved terminal modes of this job. This needs to be saved so that we can restore the
    /// terminal to the same state after temporarily taking control over the terminal when a job
    /// stops.
    struct termios tmodes {};

    /// Whether the specified job is completely constructed, i.e. completely parsed, and every
    /// process in the job has been forked, etc.
    /// This is a shared_ptr because it may be passed to child jobs through the lineage.
    const std::shared_ptr<relaxed_atomic_bool_t> constructed =
        std::make_shared<relaxed_atomic_bool_t>(false);

    /// Whether the root job is constructed; this may share a reference with 'constructed'.
    const std::shared_ptr<relaxed_atomic_bool_t> root_constructed;

    /// Flags associated with the job.
    struct flags_t {
        /// Whether the user has been told about stopped job.
        bool notified{false};

        /// Whether this job is in the foreground.
        bool foreground{false};

        /// Whether the exit status should be negated. This flag can only be set by the not builtin.
        bool negate{false};

        /// This job is disowned, and should be removed from the active jobs list.
        bool disown_requested{false};

        /// Whether to print timing for this job.
        bool has_time_prefix{false};
    } job_flags{};

    /// Access the job flags.
    const flags_t &flags() const { return job_flags; }

    /// Access mutable job flags.
    flags_t &mut_flags() { return job_flags; }

    /// \return if we want job control.
    bool wants_job_control() const { return properties.job_control; }

    /// \return if this job should own the terminal when it runs.
    bool should_claim_terminal() const { return properties.wants_terminal && is_foreground(); }

    /// Mark this job as constructed. The job must not have previously been marked as constructed.
    void mark_constructed();

    /// \return whether we have internal or external procs, respectively.
    /// Internal procs are builtins, blocks, and functions.
    /// External procs include exec and external.
    bool has_internal_proc() const;
    bool has_external_proc() const;

    // Helper functions to check presence of flags on instances of jobs
    /// The job has been fully constructed, i.e. all its member processes have been launched
    bool is_constructed() const { return *constructed; }
    /// The job was launched in the foreground and has control of the terminal
    bool is_foreground() const { return flags().foreground; }
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

    /// \return whether we should report process exit events.
    /// This implements some historical behavior which has not been justified.
    bool should_report_process_exits() const;

    /// \return whether this job and its parent chain are fully constructed.
    bool job_chain_is_fully_constructed() const;

    /// Resume a (possibly) stopped job. Puts job in the foreground.  If cont is true, restore the
    /// saved terminal modes and send the process group a SIGCONT signal to wake it up before we
    /// block.
    ///
    /// \param reclaim_foreground_pgrp whether, when the job finishes or stops, to reclaim the
    /// foreground pgrp (via tcsetpgrp). \param send_sigcont Whether SIGCONT should be sent to the
    /// job if it is in the foreground.
    void continue_job(parser_t &parser, bool reclaim_foreground_pgrp, bool send_sigcont);

    /// Send the specified signal to all processes in this job.
    /// \return true on success, false on failure.
    bool signal(int signal);

    /// \returns the statuses for this job.
    statuses_t get_statuses() const;
};

/// Whether this shell is attached to the keyboard at all.
enum class session_interactivity_t { not_interactive, implied, explicit_ };
session_interactivity_t session_interactivity();
void set_interactive_session(session_interactivity_t flag);

/// Whether we are a login shell.
bool get_login();
void mark_login();

/// If this flag is set, fish will never fork or run execve. It is used to put fish into a syntax
/// verifier mode where fish tries to validate the syntax of a file but doesn't actually do
/// anything.
bool no_exec();
void mark_no_exec();

// List of jobs.
typedef std::deque<shared_ptr<job_t>> job_list_t;

/// The current job control mode.
///
/// Must be one of job_control_t::all, job_control_t::interactive and job_control_t::none.
job_control_t get_job_control_mode();
void set_job_control_mode(job_control_t mode);

/// Notify the user about stopped or terminated jobs, and delete completed jobs from the job list.
/// If \p interactive is set, allow removing interactive jobs; otherwise skip them.
/// \return whether text was printed to stdout.
class parser_t;
bool job_reap(parser_t &parser, bool interactive);

/// \return the list of background jobs which we should warn the user about, if the user attempts to
/// exit. An empty result (common) means no such jobs.
job_list_t jobs_requiring_warning_on_exit(const parser_t &parser);

/// Print the exit warning for the given jobs, which should have been obtained via
/// jobs_requiring_warning_on_exit().
void print_exit_warning_for_jobs(const job_list_t &jobs);

/// Mark a process as failed to execute (and therefore completed).
void job_mark_process_as_failed(const std::shared_ptr<job_t> &job, const process_t *failed_proc);

/// Use the procfs filesystem to look up how many jiffies of cpu time was used by this process. This
/// function is only available on systems with the procfs file entry 'stat', i.e. Linux.
unsigned long proc_get_jiffies(process_t *p);

/// Update process time usage for all processes by calling the proc_get_jiffies function for every
/// process of every job.
void proc_update_jiffies(parser_t &parser);

/// Perform a set of simple sanity checks on the job list. This includes making sure that only one
/// job is in the foreground, that every process is in a valid state, etc.
void proc_sanity_check(const parser_t &parser);

/// Create a process/job exit event notification.
event_t proc_create_event(const wchar_t *msg, event_type_t type, pid_t pid, int status);

/// Initializations.
void proc_init();

/// Wait for any process finishing, or receipt of a signal.
void proc_wait_any(parser_t &parser);

/// Set and get whether we are in initialization.
// Hackish. In order to correctly report the origin of code with no associated file, we need to
// know whether it's run during initialization or not.
void set_is_within_fish_initialization(bool flag);
bool is_within_fish_initialization();

/// Send SIGHUP to the list \p jobs, excepting those which are in fish's pgroup.
void hup_jobs(const job_list_t &jobs);

/// Give ownership of the terminal to the specified job, if it wants it.
///
/// \param j The job to give the terminal to.
/// \param continuing_from_stopped If this variable is set, we are giving back control to a job that
/// was previously stopped. In that case, we need to set the terminal attributes to those saved in
/// the job.
/// \return 1 if transferred, 0 if no transfer was necessary, -1 on error.
int terminal_maybe_give_to_job(const job_t *j, bool continuing_from_stopped);

/// Add a pid to the list of pids we wait on even though they are not associated with any jobs.
/// Used to avoid zombie processes after disown.
void add_disowned_pgid(pid_t pgid);

bool have_proc_stat();

#endif
