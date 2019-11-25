// The fish parser.
#ifndef FISH_PARSER_H
#define FISH_PARSER_H

#include <stddef.h>
#include <unistd.h>

#include <csignal>
#include <list>
#include <memory>
#include <type_traits>
#include <vector>

#include "common.h"
#include "event.h"
#include "expand.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "proc.h"

class io_chain_t;

/// event_blockage_t represents a block on events.
struct event_blockage_t {};

typedef std::list<event_blockage_t> event_blockage_list_t;

inline bool event_block_list_blocks_type(const event_blockage_list_t &ebls) {
    return !ebls.empty();
}

/// Types of blocks.
enum block_type_t {
    WHILE,                    /// While loop block
    FOR,                      /// For loop block
    IF,                       /// If block
    FUNCTION_CALL,            /// Function invocation block
    FUNCTION_CALL_NO_SHADOW,  /// Function invocation block with no variable shadowing
    SWITCH,                   /// Switch block
    SUBST,                    /// Command substitution scope
    TOP,                      /// Outermost block
    BEGIN,                    /// Unconditional block
    SOURCE,                   /// Block created by the . (source) builtin
    EVENT,                    /// Block created on event notifier invocation
    BREAKPOINT,               /// Breakpoint block
    VARIABLE_ASSIGNMENT,      /// Variable assignment before a command
};

/// Possible states for a loop.
enum class loop_status_t {
    normals,    /// current loop block executed as normal
    breaks,     /// current loop block should be removed
    continues,  /// current loop block should be skipped
};

/// block_t represents a block of commands.
class block_t {
    /// Construct from a block type.
    explicit block_t(block_type_t t);

    /// Type of block.
    const block_type_t block_type;

   public:
    /// Name of file that created this block. This string is intern'd.
    const wchar_t *src_filename{nullptr};
    /// Line number where this block was created.
    int src_lineno{0};
    /// Whether we should pop the environment variable stack when we're popped off of the block
    /// stack.
    bool wants_pop_env{false};
    /// List of event blocks.
    event_blockage_list_t event_blocks{};

    // If this is a function block, the function name and arguments.
    // Otherwise empty.
    wcstring function_name{};
    wcstring_list_t function_args{};

    // If this is a source block, the source'd file, interned.
    // Otherwise nothing.
    const wchar_t *sourced_file{};

    // If this is an event block, the event. Otherwise ignored.
    maybe_t<event_t> event;

    block_type_t type() const { return this->block_type; }

    /// Description of the block, for debugging.
    wcstring description() const;

    /// Entry points for creating blocks.
    static block_t if_block();
    static block_t event_block(event_t evt);
    static block_t function_block(wcstring name, wcstring_list_t args, bool shadows);
    static block_t source_block(const wchar_t *src);
    static block_t for_block();
    static block_t while_block();
    static block_t switch_block();
    static block_t scope_block(block_type_t type);
    static block_t breakpoint_block();
    static block_t variable_assignment_block();

    ~block_t();
};

struct profile_item_t {
    /// Time spent executing the specified command, including parse time for nested blocks.
    int exec;
    /// Time spent parsing the specified command, including execution time for command
    /// substitutions.
    int parse;
    /// The block level of the specified command. nested blocks and command substitutions both
    /// increase the block level.
    size_t level;
    /// If the execution of this command was skipped.
    bool skipped;
    /// The command string.
    wcstring cmd;
};

class parse_execution_context_t;
class completion_t;
struct event_t;

/// Miscellaneous data used to avoid recursion and others.
struct library_data_t {
    /// A counter incremented every time a command executes.
    uint64_t exec_count{0};

    /// Last reader run count.
    uint64_t last_exec_run_counter{UINT64_MAX};

    /// Number of recursive calls to builtin_complete().
    uint32_t builtin_complete_recursion_level{0};

    /// Whether we called builtin_complete -C without parameter.
    bool builtin_complete_current_commandline{false};

    /// Whether we are currently cleaning processes.
    bool is_cleaning_procs{false};

    /// The job id of the job being populated.
    /// This supports the '--on-job-exit caller' feature.
    job_id_t caller_job_id{-1};

    /// Whether we are running a subshell command.
    bool is_subshell{false};

    /// Whether we are running a block of commands.
    bool is_block{false};

    /// Whether we are running due to a `breakpoint` command.
    bool is_breakpoint{false};

    /// Whether we are running an event handler. This is not a bool because we keep count of the
    /// event nesting level.
    int is_event{0};

    /// Whether we are currently interactive.
    bool is_interactive{false};

    /// Whether to suppress fish_trace output. This occurs in the prompt, event handlers, and key
    /// bindings.
    bool suppress_fish_trace{false};

    /// Whether we should break or continue the current loop.
    enum loop_status_t loop_status { loop_status_t::normals };

    /// Whether we should return from the current function.
    bool returning{false};

    /// The current filename we are evaluating, either from builtin source or on the command line.
    /// This is an intern'd string.
    const wchar_t *current_filename{};

    /// List of events that have been sent but have not yet been delivered because they are blocked.
    std::vector<shared_ptr<event_t>> blocked_events{};

    /// A stack of fake values to be returned by builtin_commandline. This is used by the completion
    /// machinery when wrapping: e.g. if `tig` wraps `git` then git completions need to see git on
    /// the command line.
    wcstring_list_t transient_commandlines{};

    /// A file descriptor holding the current working directory, for use in openat().
    /// This is never null and never invalid.
    std::shared_ptr<const autoclose_fd_t> cwd_fd{};
};

class parser_t : public std::enable_shared_from_this<parser_t> {
    friend class parse_execution_context_t;

   private:
    /// Indication that we should skip all blocks.
    volatile sig_atomic_t cancellation_requested = false;
    /// The current execution context.
    std::unique_ptr<parse_execution_context_t> execution_context;
    /// The jobs associated with this parser.
    job_list_t job_list;
    /// The list of blocks. This is a deque because we give out raw pointers to callers, who hold
    /// them across manipulating this stack.
    std::deque<block_t> block_stack;
    /// The 'depth' of the fish call stack.
    int eval_level = -1;
    /// Set of variables for the parser.
    const std::shared_ptr<env_stack_t> variables;
    /// Miscellaneous library data.
    library_data_t library_data{};

    /// List of profile items
    /// These are pointers because we return pointers to them to callers,
    /// who may hold them across blocks (which would cause reallocations internal
    /// to profile_items)
    std::vector<std::unique_ptr<profile_item_t>> profile_items;

    // No copying allowed.
    parser_t(const parser_t &);
    parser_t &operator=(const parser_t &);

    /// Adds a job to the beginning of the job list.
    void job_add(shared_ptr<job_t> job);

    /// Returns the name of the currently evaluated function if we are currently evaluating a
    /// function, null otherwise. This is tested by moving down the block-scope-stack, checking
    /// every block if it is of type FUNCTION_CALL.
    const wchar_t *is_function(size_t idx = 0) const;

    // Given a file path, return something nicer. Currently we just "unexpand" tildes.
    wcstring user_presentable_path(const wcstring &path) const;

    /// Helper for stack_trace().
    void stack_trace_internal(size_t block_idx, wcstring *out) const;

    /// Create a parser.
    parser_t();
    parser_t(std::shared_ptr<env_stack_t> vars);

    /// The main parser.
    static std::shared_ptr<parser_t> principal;

   public:
    /// Get the "principal" parser, whatever that is.
    static parser_t &principal_parser();

    /// Indicates that execution of all blocks in the principal parser should stop. This is called
    /// from signal handlers!
    static void skip_all_blocks();

    /// Global event blocks.
    event_blockage_list_t global_event_blocks;

    /// Evaluate the expressions contained in cmd.
    ///
    /// \param cmd the string to evaluate
    /// \param io io redirections to perform on all started jobs
    /// \param block_type The type of block to push on the block stack
    ///
    /// \return 0 on success, 1 on a parse error.
    int eval(wcstring cmd, const io_chain_t &io, enum block_type_t block_type);

    /// Evaluate the parsed source ps.
    /// \return 0 on success, 1 on a parse error.
    int eval(parsed_source_ref_t ps, const io_chain_t &io, enum block_type_t block_type);

    /// Evaluates a node.
    /// The node type must be grammar::statement or grammar::job_list.
    template <typename T>
    int eval_node(parsed_source_ref_t ps, tnode_t<T> node, const io_chain_t &io,
                  block_type_t block_type, std::shared_ptr<job_t> parent_job);

    /// Evaluate line as a list of parameters, i.e. tokenize it and perform parameter expansion and
    /// cmdsubst execution on the tokens. Errors are ignored. If a parser is provided, it is used
    /// for command substitution expansion.
    static std::vector<completion_t> expand_argument_list(const wcstring &arg_list_src,
                                                          expand_flags_t flags,
                                                          const environment_t &vars,
                                                          const std::shared_ptr<parser_t> &parser);

    /// Returns a string describing the current parser position in the format 'FILENAME (line
    /// LINE_NUMBER): LINE'. Example:
    ///
    /// init.fish (line 127): ls|grep pancake
    wcstring current_line();

    /// Returns the current line number.
    int get_lineno() const;

    /// Returns the block at the given index. 0 corresponds to the innermost block. Returns NULL
    /// when idx is at or equal to the number of blocks.
    const block_t *block_at_index(size_t idx) const;
    block_t *block_at_index(size_t idx);

    /// Returns the current (innermost) block.
    block_t *current_block();

    /// Count of blocks.
    size_t block_count() const { return block_stack.size(); }

    /// Get the list of jobs.
    job_list_t &jobs() { return job_list; }
    const job_list_t &jobs() const { return job_list; }

    /// Get the variables.
    env_stack_t &vars() { return *variables; }
    const env_stack_t &vars() const { return *variables; }

    /// Get the library data.
    library_data_t &libdata() { return library_data; }
    const library_data_t &libdata() const { return library_data; }

    /// Get and set the last proc statuses.
    int get_last_status() const { return vars().get_last_status(); }
    statuses_t get_last_statuses() const { return vars().get_last_statuses(); }
    void set_last_statuses(statuses_t s) { vars().set_last_statuses(std::move(s)); }

    /// Pushes a new block. Returns a pointer to the block, stored in the parser. The pointer is
    /// valid until the call to pop_block()
    block_t *push_block(block_t &&b);

    /// Remove the outermost block, asserting it's the given one.
    void pop_block(const block_t *b);

    /// Return a description of the given blocktype.
    const wchar_t *get_block_desc(int block) const;

    /// Return the function name for the specified stack frame. Default is one (current frame).
    const wchar_t *get_function_name(int level = 1);

    /// Promotes a job to the front of the list.
    void job_promote(job_t *job);

    /// Return the job with the specified job id. If id is 0 or less, return the last job used.
    job_t *job_get(job_id_t job_id);

    /// Returns the job with the given pid.
    job_t *job_get_from_pid(pid_t pid) const;

    /// Returns a new profile item if profiling is active. The caller should fill it in. The
    /// parser_t will clean it up.
    profile_item_t *create_profile_item();

    void get_backtrace(const wcstring &src, const parse_error_list_t &errors,
                       wcstring &output) const;

    /// Output profiling data to the given filename.
    void emit_profiling(const char *path) const;

    /// Returns the file currently evaluated by the parser. This can be different than
    /// reader_current_filename, e.g. if we are evaulating a function defined in a different file
    /// than the one curently read.
    const wchar_t *current_filename() const;

    /// Return if we are interactive, which means we are executing a command that the user typed in
    /// (and not, say, a prompt).
    bool is_interactive() const { return libdata().is_interactive; }

    /// Return a string representing the current stack trace.
    wcstring stack_trace() const;

    /// \return whether the number of functions in the stack exceeds our stack depth limit.
    bool function_stack_is_overflowing() const;

    /// \return a shared pointer reference to this parser.
    std::shared_ptr<parser_t> shared();

    ~parser_t();
};

#endif
