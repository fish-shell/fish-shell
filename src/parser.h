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
struct event_blockage_t {
};

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
};

/// Possible states for a loop.
enum loop_status_t {
    LOOP_NORMAL,    /// current loop block executed as normal
    LOOP_BREAK,     /// current loop block should be removed
    LOOP_CONTINUE,  /// current loop block should be skipped
};

/// block_t represents a block of commands.
struct block_t {
   protected:
    /// Protected constructor. Use one of the subclasses below.
    explicit block_t(block_type_t t);

   private:
    /// Type of block.
    const block_type_t block_type;

   public:
    /// Whether execution of the commands in this block should be skipped.
    bool skip{false};
    /// Status for the current loop block. Can be any of the values from the loop_status enum.
    enum loop_status_t loop_status { LOOP_NORMAL };
    /// The job that is currently evaluated in the specified block.
    shared_ptr<job_t> job{};
    /// Name of file that created this block. This string is intern'd.
    const wchar_t *src_filename{nullptr};
    /// Line number where this block was created.
    int src_lineno{0};
    /// Whether we should pop the environment variable stack when we're popped off of the block
    /// stack.
    bool wants_pop_env{false};
    /// List of event blocks.
    event_blockage_list_t event_blocks{};

    block_type_t type() const { return this->block_type; }

    /// Description of the block, for debugging.
    wcstring description() const;

    /// Destructor
    virtual ~block_t();
};

struct if_block_t : public block_t {
    if_block_t();
};

struct event_block_t : public block_t {
    event_t const event;
    explicit event_block_t(const event_t &evt);
};

struct function_block_t : public block_t {
    const process_t *process;
    wcstring name;
    function_block_t(const process_t *p, wcstring n, bool shadows);
};

struct source_block_t : public block_t {
    const wchar_t *const source_file;
    explicit source_block_t(const wchar_t *src);
};

struct for_block_t : public block_t {
    for_block_t();
};

struct while_block_t : public block_t {
    while_block_t();
};

struct switch_block_t : public block_t {
    switch_block_t();
};

struct scope_block_t : public block_t {
    explicit scope_block_t(block_type_t type);  // must be BEGIN, TOP or SUBST
};

struct breakpoint_block_t : public block_t {
    breakpoint_block_t();
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

class parser_t : public std::enable_shared_from_this<parser_t> {
    friend class parse_execution_context_t;

   private:
    /// Indication that we should skip all blocks.
    volatile sig_atomic_t cancellation_requested = false;
    /// The current execution context.
    std::unique_ptr<parse_execution_context_t> execution_context;
    /// List of called functions, used to help prevent infinite recursion.
    wcstring_list_t forbidden_function;
    /// The jobs associated with this parser.
    job_list_t my_job_list;
    /// The list of blocks
    std::vector<std::unique_ptr<block_t>> block_stack;
    /// The 'depth' of the fish call stack.
    int eval_level = -1;
    /// Set of variables for the parser.
    env_stack_t &variables;
#if 0
// TODO: Lint says this isn't used (which is true). Should this be removed?
    /// Gets a description of the block stack, for debugging.
    wcstring block_stack_description() const;
#endif

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

    /// Helper for push_block()
    void push_block_int(block_t *b);

    /// Create a parser.
    parser_t();

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
    int eval(const wcstring &cmd, const io_chain_t &io, enum block_type_t block_type);

    /// Evaluate the parsed source ps.
    void eval(parsed_source_ref_t ps, const io_chain_t &io, enum block_type_t block_type);

    /// Evaluates a node.
    /// The node type must be grammar::statement or grammar::job_list.
    template <typename T>
    int eval_node(parsed_source_ref_t ps, tnode_t<T> node, const io_chain_t &io,
                  block_type_t block_type, std::shared_ptr<job_t> parent_job);

    /// Evaluate line as a list of parameters, i.e. tokenize it and perform parameter expansion and
    /// cmdsubst execution on the tokens. The output is inserted into output. Errors are ignored.
    ///
    /// \param arg_src String to evaluate as an argument list
    /// \param flags Some expand flags to use
    /// \param output List to insert output into
    static void expand_argument_list(const wcstring &arg_src, expand_flags_t flags,
                                     const environment_t &vars, std::vector<completion_t> *output);

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
    job_list_t &job_list() { return my_job_list; }

    /// Get the variables.
    env_stack_t &vars() { return variables; }
    const env_stack_t &vars() const { return variables; }

    /// Pushes a new block created with the given arguments
    /// Returns a pointer to the block. The pointer is valid
    /// until the call to pop_block()
    template <typename BLOCKTYPE, typename... Args>
    BLOCKTYPE *push_block(Args &&... args) {
        BLOCKTYPE *ret = new BLOCKTYPE(std::forward<Args>(args)...);
        this->push_block_int(ret);
        return ret;
    }

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

    /// Detect errors in the specified string when parsed as an argument list. Returns true if an
    /// error occurred.
    bool detect_errors_in_argument_list(const wcstring &arg_list_src, wcstring *out_err,
                                        const wchar_t *prefix) const;

    /// Tell the parser that the specified function may not be run if not inside of a conditional
    /// block. This is to remove some possibilities of infinite recursion.
    void forbid_function(const wcstring &function);

    /// Undo last call to parser_forbid_function().
    void allow_function();

    /// Output profiling data to the given filename.
    void emit_profiling(const char *path) const;

    /// Returns the file currently evaluated by the parser. This can be different than
    /// reader_current_filename, e.g. if we are evaulating a function defined in a different file
    /// than the one curently read.
    const wchar_t *current_filename() const;

    /// Return a string representing the current stack trace.
    wcstring stack_trace() const;

    ~parser_t();
};

#endif
