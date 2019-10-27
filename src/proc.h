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

    /// \return if we are stopped (as in SIGSTOP).
    bool stopped() const { return WIFSTOPPED(status_); }

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

    io_chain_t process_io_chain;

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

    /// Sets argv.
    void set_argv(const wcstring_list_t &argv) { argv_array.set(argv); }

    /// Returns argv.
    wchar_t **get_argv() { return argv_array.get(); }
    const null_terminated_array_t<wchar_t> &get_argv_array() const { return argv_array; }

    /// Returns argv[idx].
    const wchar_t *argv(size_t idx) const {
        const wchar_t *const *argv = argv_array.get();
        assert(argv != NULL);
        return argv[idx];
    }

    /// Returns argv[0], or NULL.
    const wchar_t *argv0() const {
        const wchar_t *const *argv = argv_array.get();
        return argv ? argv[0] : NULL;
    }

    /// IO chain getter and setter.
    const io_chain_t &io_chain() const { return process_io_chain; }

    void set_io_chain(io_chain_t chain) { this->process_io_chain = std::move(chain); }

    /// Store the current topic generations. That is, right before the process is launched, record
    /// the generations of all topics; then we can tell which generation values have changed after
    /// launch. This helps us avoid spurious waitpid calls.
    void check_generations_before_launch();

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

typedef int job_id_t;
job_id_t acquire_job_id(void);
void release_job_id(job_id_t jobid);

/// A struct represeting a job. A job is basically a pipeline of one or more processes and a couple
/// of flags.
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
    };

   private:
    /// Set of immutable job properties.
    const properties_t properties;

    /// The original command which led to the creation of this job. It is used for displaying
    /// messages about job status on the terminal.
    wcstring command_str;

    // The IO chain associated with the block.
    const io_chain_t block_io;

    // The parent job. If we were created as a nested job due to execution of a block or function in
    // a pipeline, then this refers to the job corresponding to that pipeline. Otherwise it is null.
    const std::shared_ptr<job_t> parent_job;

    // No copying.
    job_t(const job_t &rhs) = delete;
    void operator=(const job_t &) = delete;

   public:
    job_t(job_id_t job_id, const properties_t &props, io_chain_t bio,
          std::shared_ptr<job_t> parent);
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

    /// The id of this job.
    const job_id_t job_id;

    /// The saved terminal modes of this job. This needs to be saved so that we can restore the
    /// terminal to the same state after temporarily taking control over the terminal when a job
    /// stops.
    struct termios tmodes {};

    /// Flags associated with the job.
    struct flags_t {
        /// Whether the user has been told about stopped job.
        bool notified{false};

        /// Whether this job is in the foreground.
        bool foreground{false};

        /// Whether the specified job is completely constructed, i.e. completely parsed, and every
        /// process in the job has been forked, etc.
        bool constructed{false};

        /// Whether the exit status should be negated. This flag can only be set by the not builtin.
        bool negate{false};

        /// Whether the job is under job control, i.e. has its own pgrp.
        bool job_control{false};

        /// This job is disowned, and should be removed from the active jobs list.
        bool disown_requested{false};
    } job_flags{};

    /// Access the job flags.
    const flags_t &flags() const { return job_flags; }

    /// Access mutable job flags.
    flags_t &mut_flags() { return job_flags; }

    /// \return if we want job control.
    bool wants_job_control() const { return flags().job_control; }

    /// \return if this job should own the terminal when it runs.
    bool should_claim_terminal() const { return properties.wants_terminal && is_foreground(); }

    /// Returns the block IO redirections associated with the job. These are things like the IO
    /// redirections associated with the begin...end statement.
    const io_chain_t &block_io_chain() const { return this->block_io; }

    /// Fetch all the IO redirections associated with the job.
    io_chain_t all_io_redirections() const;

    // Helper functions to check presence of flags on instances of jobs
    /// The job has been fully constructed, i.e. all its member processes have been launched
    bool is_constructed() const { return flags().constructed; }
    /// The job was launched in the foreground and has control of the terminal
    bool is_foreground() const { return flags().foreground; }
    /// The job is complete, i.e. all its member processes have been reaped
    bool is_completed() const;
    /// The job is in a stopped state
    bool is_stopped() const;
    /// The job is OK to be externally visible, e.g. to the user via `jobs`
    bool is_visible() const {
        return !is_completed() && is_constructed() && !flags().disown_requested;
    };
    bool skip_notification() const { return properties.skip_notification; }
    bool from_event_handler() const { return properties.from_event_handler; }

    /// \return whether we should report process exit events.
    /// This implements some historical behavior which has not been justified.
    bool should_report_process_exits() const;

    /// \return the parent job, or nullptr.
    const std::shared_ptr<job_t> get_parent() const { return parent_job; }

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

    /// Return the job instance matching this unique job id.
    /// If id is 0 or less, return the last job used.
    static job_t *from_job_id(job_id_t id);

    /// Return the job containing the process identified by the unique pid provided.
    static job_t *from_pid(pid_t pid);
};

/// Whether this shell is attached to the keyboard at all.
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

/// Mark a process as failed to execute (and therefore completed).
void job_mark_process_as_failed(const std::shared_ptr<job_t> &job, const process_t *p);

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

/// Terminate all background jobs
void hup_background_jobs(const parser_t &parser);

/// Give ownership of the terminal to the specified job, if it wants it.
///
/// \param j The job to give the terminal to.
/// \param continuing_from_stopped If this variable is set, we are giving back control to a job that
/// was previously stopped. In that case, we need to set the terminal attributes to those saved in
/// the job.
/// \return 1 if transferred, 0 if no transfer was necessary, -1 on error.
int terminal_maybe_give_to_job(const job_t *j, bool continuing_from_stopped);

/// Given that we are about to run a builtin, acquire the terminal if it is owned by the given job.
/// Returns the pid to restore after running the builtin, or -1 if there is no pid to restore.
pid_t terminal_acquire_before_builtin(int job_pgid);

/// Add a pid to the list of pids we wait on even though they are not associated with any jobs.
/// Used to avoid zombie processes after disown.
void add_disowned_pgid(pid_t pgid);

bool have_proc_stat();

#endif
