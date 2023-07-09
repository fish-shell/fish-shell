// The fish parser.
#ifndef FISH_PARSER_H
#define FISH_PARSER_H

#include <stddef.h>

#include <csignal>
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "common.h"
#include "cxx.h"
#include "env.h"
#include "event.h"
#include "expand.h"
#include "maybe.h"
#include "operation_context.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "proc.h"
#include "util.h"
#include "wait_handle.h"

class autoclose_fd_t;
class io_chain_t;
struct Event;
struct job_group_t;
class parser_t;

/// Types of blocks.
enum class block_type_t : uint8_t {
    while_block,              /// While loop block
    for_block,                /// For loop block
    if_block,                 /// If block
    function_call,            /// Function invocation block
    function_call_no_shadow,  /// Function invocation block with no variable shadowing
    switch_block,             /// Switch block
    subst,                    /// Command substitution scope
    top,                      /// Outermost block
    begin,                    /// Unconditional block
    source,                   /// Block created by the . (source) builtin
    event,                    /// Block created on event notifier invocation
    breakpoint,               /// Breakpoint block
    variable_assignment,      /// Variable assignment before a command
};

/// Possible states for a loop.
enum class loop_status_t {
    normals,    /// current loop block executed as normal
    breaks,     /// current loop block should be removed
    continues,  /// current loop block should be skipped
};

/// block_t represents a block of commands.
class block_t {
   public:
    /// Construct from a block type.
    explicit block_t(block_type_t t);

    // If this is a function block, the function name. Otherwise empty.
    wcstring function_name{};

    /// List of event blocks.
    uint64_t event_blocks{};

    // If this is a function block, the function args. Otherwise empty.
    std::vector<wcstring> function_args{};

    /// Name of file that created this block.
    filename_ref_t src_filename{};

    // If this is an event block, the event. Otherwise ignored.
    std::shared_ptr<rust::Box<Event>> event;

    // If this is a source block, the source'd file, interned.
    // Otherwise nothing.
    filename_ref_t sourced_file{};

    /// Line number where this block was created.
    int src_lineno{0};

   private:
    /// Type of block.
    const block_type_t block_type;

   public:
    /// Whether we should pop the environment variable stack when we're popped off of the block
    /// stack.
    bool wants_pop_env{false};

    /// Description of the block, for debugging.
    wcstring description() const;

    block_type_t type() const { return this->block_type; }

    /// \return if we are a function call (with or without shadowing).
    bool is_function_call() const;

    /// Entry points for creating blocks.
    static block_t if_block();
    static block_t event_block(const void *evt_);
    static block_t function_block(wcstring name, std::vector<wcstring> args, bool shadows);
    static block_t source_block(filename_ref_t src);
    static block_t for_block();
    static block_t while_block();
    static block_t switch_block();
    static block_t scope_block(block_type_t type);
    static block_t breakpoint_block();
    static block_t variable_assignment_block();

    /// autocxx junk.
    void ffi_incr_event_blocks();
    uint64_t ffi_event_blocks() const { return event_blocks; }
};

struct profile_item_t {
    using microseconds_t = long long;

    /// Time spent executing the command, including nested blocks.
    microseconds_t duration{};

    /// The block level of the specified command. Nested blocks and command substitutions both
    /// increase the block level.
    size_t level{};

    /// If the execution of this command was skipped.
    bool skipped{};

    /// The command string.
    wcstring cmd{};

    /// \return the current time as a microsecond timestamp since the epoch.
    static microseconds_t now() { return get_time(); }
};

class parse_execution_context_t;

/// Plain-Old-Data components of `struct library_data_t` that can be shared over FFI
struct library_data_pod_t {
    /// A counter incremented every time a command executes.
    uint64_t exec_count{0};

    /// A counter incremented every time a command produces a $status.
    uint64_t status_count{0};

    /// Last reader run count.
    uint64_t last_exec_run_counter{UINT64_MAX};

    /// Number of recursive calls to the internal completion function.
    uint32_t complete_recursion_level{0};

    /// If set, we are currently within fish's initialization routines.
    bool within_fish_init{false};

    /// If we're currently repainting the commandline.
    /// Useful to stop infinite loops.
    bool is_repaint{false};

    /// Whether we called builtin_complete -C without parameter.
    bool builtin_complete_current_commandline{false};

    /// Whether we are currently cleaning processes.
    bool is_cleaning_procs{false};

    /// The internal job id of the job being populated, or 0 if none.
    /// This supports the '--on-job-exit caller' feature.
    internal_job_id_t caller_id{0};

    /// Whether we are running a subshell command.
    bool is_subshell{false};

    /// Whether we are running an event handler. This is not a bool because we keep count of the
    /// event nesting level.
    int is_event{0};

    /// Whether we are currently interactive.
    bool is_interactive{false};

    /// Whether to suppress fish_trace output. This occurs in the prompt, event handlers, and key
    /// bindings.
    bool suppress_fish_trace{false};

    /// Whether we should break or continue the current loop.
    /// This is set by the 'break' and 'continue' commands.
    enum loop_status_t loop_status { loop_status_t::normals };

    /// Whether we should return from the current function.
    /// This is set by the 'return' command.
    bool returning{false};

    /// Whether we should stop executing.
    /// This is set by the 'exit' command, and unset after 'reader_read'.
    /// Note this only exits up to the "current script boundary." That is, a call to exit within a
    /// 'source' or 'read' command will only exit up to that command.
    bool exit_current_script{false};

    /// The read limit to apply to captured subshell output, or 0 for none.
    size_t read_limit{0};
};

/// Miscellaneous data used to avoid recursion and others.
struct library_data_t : public library_data_pod_t {
    /// The current filename we are evaluating, either from builtin source or on the command line.
    filename_ref_t current_filename{};

    /// A stack of fake values to be returned by builtin_commandline. This is used by the completion
    /// machinery when wrapping: e.g. if `tig` wraps `git` then git completions need to see git on
    /// the command line.
    std::vector<wcstring> transient_commandlines{};

    /// A file descriptor holding the current working directory, for use in openat().
    /// This is never null and never invalid.
    std::shared_ptr<const autoclose_fd_t> cwd_fd{};

    /// Status variables set by the main thread as jobs are parsed and read by various consumers.
    struct {
        /// Used to get the head of the current job (not the current command, at least for now)
        /// for `status current-command`.
        wcstring command;
        /// Used to get the full text of the current job for `status current-commandline`.
        wcstring commandline;
    } status_vars;

    public:
    wcstring get_status_vars_command() const { return status_vars.command; }
    wcstring get_status_vars_commandline() const { return status_vars.commandline; }
};

/// The result of parser_t::eval family.
struct eval_res_t {
    /// The value for $status.
    proc_status_t status;

    /// If set, there was an error that should be considered a failed expansion, such as
    /// command-not-found. For example, `touch (not-a-command)` will not invoke 'touch' because
    /// command-not-found will mark break_expand.
    bool break_expand{false};

    /// If set, no commands were executed and there we no errors.
    bool was_empty{false};

    /// If set, no commands produced a $status value.
    bool no_status{false};

    /* implicit */ eval_res_t(proc_status_t status, bool break_expand = false,
                              bool was_empty = false, bool no_status = false)
        : status(status), break_expand(break_expand), was_empty(was_empty), no_status(no_status) {}
};

enum class parser_status_var_t : uint8_t {
    current_command,
    current_commandline,
    count_,
};

class parser_t : public std::enable_shared_from_this<parser_t> {
    friend class parse_execution_context_t;

   private:
    /// The current execution context.
    std::unique_ptr<parse_execution_context_t> execution_context;

    /// The jobs associated with this parser.
    job_list_t job_list;

    /// Our store of recorded wait-handles. These are jobs that finished in the background, and have
    /// been reaped, but may still be wait'ed on.
    rust::Box<WaitHandleStoreFFI> wait_handles;

    /// The list of blocks. This is a deque because we give out raw pointers to callers, who hold
    /// them across manipulating this stack.
    /// This is in "reverse" order: the topmost block is at the front. This enables iteration from
    /// top down using range-based for loops.
    std::deque<block_t> block_list;

    /// The 'depth' of the fish call stack.
    int eval_level = -1;

    /// Set of variables for the parser.
    const std::shared_ptr<env_stack_t> variables;

    /// Miscellaneous library data.
    library_data_t library_data{};

    /// If set, we synchronize universal variables after external commands,
    /// including sending on-variable change events.
    bool syncs_uvars_{false};

    /// If set, we are the principal parser.
    bool is_principal_{false};

    /// List of profile items.
    /// This must be a deque because we return pointers to them to callers,
    /// who may hold them across blocks (which would cause reallocations internal
    /// to profile_items). deque does not move items on reallocation.
    std::deque<profile_item_t> profile_items;

    /// Adds a job to the beginning of the job list.
    void job_add(std::shared_ptr<job_t> job);

    /// \return whether we are currently evaluating a function.
    bool is_function() const;

    /// \return whether we are currently evaluating a command substitution.
    bool is_command_substitution() const;

    /// Create a parser
    parser_t(std::shared_ptr<env_stack_t> vars, bool is_principal = false);

   public:
    // No copying allowed.
    parser_t(const parser_t &) = delete;
    parser_t &operator=(const parser_t &) = delete;

    /// Get the "principal" parser, whatever that is.
    static parser_t &principal_parser();

    /// ffi helper. Obviously this is totally bogus.
    static parser_t *principal_parser_ffi();

    /// Assert that this parser is allowed to execute on the current thread.
    void assert_can_execute() const;

    /// Global event blocks.
    uint64_t global_event_blocks{};

    eval_res_t eval(const wcstring &cmd, const io_chain_t &io);

    /// Evaluate the expressions contained in cmd.
    ///
    /// \param cmd the string to evaluate
    /// \param io io redirections to perform on all started jobs
    /// \param job_group if set, the job group to give to spawned jobs.
    /// \param block_type The type of block to push on the block stack, which must be either 'top'
    /// or 'subst'.
    /// \return the result of evaluation.
    eval_res_t eval_with(const wcstring &cmd, const io_chain_t &io,
                         const job_group_ref_t &job_group, block_type_t block_type);

    eval_res_t eval_string_ffi1(const wcstring &cmd);

    /// Evaluate the parsed source ps.
    /// Because the source has been parsed, a syntax error is impossible.
    eval_res_t eval_parsed_source(const parsed_source_ref_t &ps, const io_chain_t &io,
                                  const job_group_ref_t &job_group = {},
                                  block_type_t block_type = block_type_t::top);

    /// Evaluates a node.
    /// The node type must be ast_t::statement_t or ast::job_list_t.
    template <typename T>
    eval_res_t eval_node(const parsed_source_ref_t &ps, const T &node, const io_chain_t &block_io,
                         const job_group_ref_t &job_group,
                         block_type_t block_type = block_type_t::top);

    /// Evaluate line as a list of parameters, i.e. tokenize it and perform parameter expansion and
    /// cmdsubst execution on the tokens. Errors are ignored. If a parser is provided, it is used
    /// for command substitution expansion.
    static completion_list_t expand_argument_list(const wcstring &arg_list_src,
                                                  expand_flags_t flags,
                                                  const operation_context_t &ctx);

    /// Returns a string describing the current parser position in the format 'FILENAME (line
    /// LINE_NUMBER): LINE'. Example:
    ///
    /// init.fish (line 127): ls|grep pancake
    wcstring current_line();

    /// Returns the current line number.
    int get_lineno() const;

    /// \return whether we are currently evaluating a "block" such as an if statement.
    /// This supports 'status is-block'.
    bool is_block() const;

    /// \return whether we have a breakpoint block.
    bool is_breakpoint() const;

    /// Returns the block at the given index. 0 corresponds to the innermost block. Returns nullptr
    /// when idx is at or equal to the number of blocks.
    const block_t *block_at_index(size_t idx) const;
    block_t *block_at_index(size_t idx);

    /// Return the list of blocks. The first block is at the top.
    const std::deque<block_t> &blocks() const { return block_list; }

    size_t blocks_size() const { return block_list.size(); }

    /// Get the list of jobs.
    job_list_t &jobs() { return job_list; }
    const job_list_t &jobs() const { return job_list; }

    /// Get the variables.
    env_stack_t &vars() { return *variables; }
    const env_stack_t &vars() const { return *variables; }

    int remove_var_ffi(const wcstring &key, int mode) { return vars().remove(key, mode); }

    /// Get the library data.
    library_data_t &libdata() { return library_data; }
    const library_data_t &libdata() const { return library_data; }

    /// Get our wait handle store.
    rust::Box<WaitHandleStoreFFI> &get_wait_handles_ffi();
    const rust::Box<WaitHandleStoreFFI> &get_wait_handles_ffi() const;

    /// As get_wait_handles(), but void* pointer-to-Box to satisfy autocxx.
    void *get_wait_handles_void() const {
        const void *ptr = &get_wait_handles_ffi();
        return const_cast<void *>(ptr);
    }

    /// Get and set the last proc statuses.
    int get_last_status() const { return vars().get_last_status(); }
    statuses_t get_last_statuses() const { return vars().get_last_statuses(); }
    void set_last_statuses(statuses_t s) { vars().set_last_statuses(std::move(s)); }

    /// Cover of vars().set(), which also fires any returned event handlers.
    /// \return a value like ENV_OK.
    int set_var_and_fire(const wcstring &key, env_mode_flags_t mode, wcstring val);
    int set_var_and_fire(const wcstring &key, env_mode_flags_t mode, std::vector<wcstring> vals);

    /// Update any universal variables and send event handlers.
    /// If \p always is set, then do it even if we have no pending changes (that is, look for
    /// changes from other fish instances); otherwise only sync if this instance has changed uvars.
    void sync_uvars_and_fire(bool always = false);

    /// Pushes a new block. Returns a pointer to the block, stored in the parser. The pointer is
    /// valid until the call to pop_block().
    block_t *push_block(block_t &&b);

    /// Remove the outermost block, asserting it's the given one.
    void pop_block(const block_t *expected);

    /// Avoid maybe_t usage for ffi, sends a empty string in case of none.
    wcstring get_function_name_ffi(int level);
    /// Return the function name for the specified stack frame. Default is one (current frame).
    maybe_t<wcstring> get_function_name(int level = 1);

    /// Promotes a job to the front of the list.
    void job_promote(job_list_t::iterator job_it);
    void job_promote(const job_t *job);
    void job_promote_at(size_t job_pos);

    /// Return the job with the specified job id. If id is 0 or less, return the last job used.
    const job_t *job_with_id(job_id_t job_id) const;

    /// Returns the job with the given pid.
    job_t *job_get_from_pid(pid_t pid) const;

    /// Returns the job and position with the given pid.
    job_t *job_get_from_pid(int pid, size_t &job_pos) const;

    /// Returns a new profile item if profiling is active. The caller should fill it in.
    /// The parser_t will deallocate it.
    /// If profiling is not active, this returns nullptr.
    profile_item_t *create_profile_item();

    /// Remove the profiling items.
    void clear_profiling();

    /// Output profiling data to the given filename.
    void emit_profiling(const char *path) const;

    void get_backtrace(const wcstring &src, const parse_error_list_t &errors,
                       wcstring &output) const;

    /// Returns the file currently evaluated by the parser. This can be different than
    /// reader_current_filename, e.g. if we are evaluating a function defined in a different file
    /// than the one currently read.
    filename_ref_t current_filename() const;
    wcstring current_filename_ffi() const;

    /// Return if we are interactive, which means we are executing a command that the user typed in
    /// (and not, say, a prompt).
    bool is_interactive() const { return libdata().is_interactive; }

    /// Return a string representing the current stack trace.
    wcstring stack_trace() const;

    /// \return whether the number of functions in the stack exceeds our stack depth limit.
    bool function_stack_is_overflowing() const;

    /// Mark whether we should sync universal variables.
    void set_syncs_uvars(bool flag) { syncs_uvars_ = flag; }

    /// Set the given file descriptor as the working directory for this parser.
    /// This acquires ownership.
    void set_cwd_fd(int fd);

    /// \return a shared pointer reference to this parser.
    std::shared_ptr<parser_t> shared();

    /// \return a cancel poller for checking if this parser has been signalled.
    /// autocxx falls over with this so hide it.
#if INCLUDE_RUST_HEADERS
    cancel_checker_t cancel_checker() const;
#endif

    /// \return the operation context for this parser.
    operation_context_t context();

    /// Checks if the max eval depth has been exceeded
    bool is_eval_depth_exceeded() const { return eval_level >= FISH_MAX_EVAL_DEPTH; }

    /// autocxx junk.
    RustFFIJobList ffi_jobs() const;
    library_data_pod_t *ffi_libdata_pod();
    job_t *ffi_job_get_from_pid(int pid) const;
    const library_data_pod_t &ffi_libdata_pod_const() const;

    /// autocxx junk.
    bool ffi_has_funtion_block() const;

    /// autocxx junk.
    uint64_t ffi_global_event_blocks() const;
    void ffi_incr_global_event_blocks();
    void ffi_decr_global_event_blocks();

    /// autocxx junk.
    size_t ffi_blocks_size() const;

    ~parser_t();
};

#endif
