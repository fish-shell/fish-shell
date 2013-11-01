/** \file parser.h
  The fish parser.
*/

#ifndef FISH_PARSER_H
#define FISH_PARSER_H

#include <wchar.h>

#include "proc.h"
#include "util.h"
#include "event.h"
#include "function.h"
#include <vector>

#define PARSER_TEST_ERROR 1
#define PARSER_TEST_INCOMPLETE 2

/**
   event_blockage_t represents a block on events of the specified type
*/
struct event_blockage_t
{
    /**
       The types of events to block. This is interpreted as a bitset
       whete the value is 1 for every bit corresponding to a blocked
       event type. For example, if EVENT_VARIABLE type events should
       be blocked, (type & 1<<EVENT_BLOCKED) should be set.

       Note that EVENT_ANY can be used to specify any event.
    */
    unsigned int typemask;
};

typedef std::list<event_blockage_t> event_blockage_list_t;

inline bool event_block_list_blocks_type(const event_blockage_list_t &ebls, int type)
{
    for (event_blockage_list_t::const_iterator iter = ebls.begin(); iter != ebls.end(); ++iter)
    {
        if (iter->typemask & (1<<EVENT_ANY))
            return true;
        if (iter->typemask & (1<<type))
            return true;
    }
    return false;
}


/**
  Types of blocks
*/
enum block_type_t
{
    WHILE, /**< While loop block */
    FOR,  /**< For loop block */
    IF, /**< If block */
    FUNCTION_DEF, /**< Function definition block */
    FUNCTION_CALL, /**< Function invocation block */
    FUNCTION_CALL_NO_SHADOW, /**< Function invocation block with no variable shadowing */
    SWITCH, /**< Switch block */
    FAKE, /**< Fake block */
    SUBST, /**< Command substitution scope */
    TOP, /**< Outermost block */
    BEGIN, /**< Unconditional block */
    SOURCE, /**< Block created by the . (source) builtin */
    EVENT, /**< Block created on event notifier invocation */
    BREAKPOINT, /**< Breakpoint block */
}
;

/**
   block_t represents a block of commands.
*/
struct block_t
{
protected:
    /** Protected constructor. Use one of the subclasses below. */
    block_t(block_type_t t);

private:
    const block_type_t block_type; /**< Type of block. */
    bool made_fake;

public:
    block_type_t type() const
    {
        return this->made_fake ? FAKE : this->block_type;
    }

    /** Mark a block as fake; this is used by the return statement. */
    void mark_as_fake()
    {
        this->made_fake = true;
    }

    bool skip; /**< Whether execution of the commands in this block should be skipped */
    bool had_command; /**< Set to non-zero once a command has been executed in this block */
    int tok_pos; /**< The start index of the block */

    /**
       Status for the current loop block. Can be any of the values from the loop_status enum.
    */
    int loop_status;

    /**
       The job that is currently evaluated in the specified block.
    */
    job_t *job;

#if 0
    union
    {
        int while_state;  /**< True if the loop condition has not yet been evaluated*/
        wchar_t *for_variable; /**< Name of the variable to loop over */
        int if_state; /**< The state of the if block, can be one of IF_STATE_UNTESTED, IF_STATE_FALSE, IF_STATE_TRUE */
        wchar_t *switch_value; /**< The value to test in a switch block */
        const wchar_t *source_dest; /**< The name of the file to source*/
        event_t *event; /**<The event that triggered this block */
        wchar_t *function_call_name;
    } param1;
#endif

    /**
       Name of file that created this block
    */
    const wchar_t *src_filename;

    /**
       Line number where this block was created
    */
    int src_lineno;

    /** Whether we should pop the environment variable stack when we're popped off of the block stack */
    bool wants_pop_env;

    /** List of event blocks. */
    event_blockage_list_t event_blocks;

    /**
     Next outer block
    */
    block_t *outer;

    /** Destructor */
    virtual ~block_t();
};

struct if_block_t : public block_t
{
    bool if_expr_evaluated; // whether we've evaluated the if expression
    bool is_elseif_entry; // whether we're at the beginning of an ELSEIF branch
    bool any_branch_taken; // whether the clause of the if statement or any elseif has been found to be true
    bool else_evaluated; // whether we've encountered a terminal else block

    if_block_t();
};

struct event_block_t : public block_t
{
    event_t const event;
    event_block_t(const event_t &evt);
};

struct function_block_t : public block_t
{
    const process_t *process;
    wcstring name;
    function_block_t(const process_t *p, const wcstring &n, bool shadows);
};

struct source_block_t : public block_t
{
    const wchar_t * const source_file;
    source_block_t(const wchar_t *src);
};

struct for_block_t : public block_t
{
    wcstring variable; // the variable that will be assigned each value in the sequence
    wcstring_list_t sequence; // the sequence of values
    for_block_t(const wcstring &var);
};

struct while_block_t : public block_t
{
    int status;
    while_block_t();
};

struct switch_block_t : public block_t
{
    bool switch_taken;
    const wcstring switch_value;
    switch_block_t(const wcstring &sv);
};

struct fake_block_t : public block_t
{
    fake_block_t();
};

struct function_def_block_t : public block_t
{
    function_data_t function_data;
    function_def_block_t();
};

struct scope_block_t : public block_t
{
    scope_block_t(block_type_t type); //must be BEGIN, TOP or SUBST
};

struct breakpoint_block_t : public block_t
{
    breakpoint_block_t();
};

/**
   Possible states for a loop
*/
enum loop_status
{
    LOOP_NORMAL, /**< Current loop block executed as normal */
    LOOP_BREAK, /**< Current loop block should be removed */
    LOOP_CONTINUE, /**< Current loop block should be skipped */
};


/**
   Possible states for a while block
*/
enum while_status
{
    WHILE_TEST_FIRST, /**< This is the first command of the first lap of a while loop */
    WHILE_TEST_AGAIN, /**< This is not the first lap of the while loop, but it is the first command of the loop */
    WHILE_TESTED, /**< This is not the first command in the loop */
}
;


/**
   Errors that can be generated by the parser
*/
enum parser_error
{
    /**
       No error
    */
    NO_ERR=0,
    /**
       An error in the syntax
    */
    SYNTAX_ERROR,
    /**
       Error occured while evaluating commands
    */
    EVAL_ERROR,
    /**
       Error while evaluating cmdsubst
    */
    CMDSUBST_ERROR,
};

enum parser_type_t
{
    PARSER_TYPE_NONE,
    PARSER_TYPE_GENERAL,
    PARSER_TYPE_FUNCTIONS_ONLY,
    PARSER_TYPE_COMPLETIONS_ONLY,
    PARSER_TYPE_ERRORS_ONLY
};

struct profile_item_t
{
    /**
       Time spent executing the specified command, including parse time for nested blocks.
    */
    int exec;
    /**
       Time spent parsing the specified command, including execution time for command substitutions.
    */
    int parse;
    /**
       The block level of the specified command. nested blocks and command substitutions both increase the block level.
    */
    size_t level;
    /**
       If the execution of this command was skipped.
    */
    int skipped;
    /**
       The command string.
    */
    wcstring cmd;
};

struct tokenizer_t;

class parser_t
{
private:
    enum parser_type_t parser_type;
    std::vector<block_t> blocks;

    /** Whether or not we output errors */
    const bool show_errors;

    /** Last error code */
    int error_code;

    /** Position of last error */
    int err_pos;

    /** Description of last error */
    wcstring err_buff;

    /** Pointer to the current tokenizer */
    tokenizer_t *current_tokenizer;

    /** String for representing the current line */
    wcstring lineinfo;

    /** This is the position of the beginning of the currently parsed command */
    int current_tokenizer_pos;

    /** List of called functions, used to help prevent infinite recursion */
    wcstring_list_t forbidden_function;

    /** String index where the current job started. */
    int job_start_pos;

    /** The jobs associated with this parser */
    job_list_t my_job_list;

    /**
       Keeps track of how many recursive eval calls have been made. Eval
       doesn't call itself directly, recursion happens on blocks and on
       command substitutions.
    */
    int eval_level;

    /* No copying allowed */
    parser_t(const parser_t&);
    parser_t& operator=(const parser_t&);

    void parse_job_argument_list(process_t *p, job_t *j, tokenizer_t *tok, std::vector<completion_t>&, bool);
    int parse_job(process_t *p, job_t *j, tokenizer_t *tok);
    void skipped_exec(job_t * j);
    void eval_job(tokenizer_t *tok);
    int parser_test_argument(const wchar_t *arg, wcstring *out, const wchar_t *prefix, int offset);
    void print_errors(wcstring &target, const wchar_t *prefix);
    void print_errors_stderr();

    /** Create a job */
    job_t *job_create();

public:
    std::vector<profile_item_t*> profile_items;

    /**
       Returns the name of the currently evaluated function if we are
       currently evaluating a function, null otherwise. This is tested by
       moving down the block-scope-stack, checking every block if it is of
       type FUNCTION_CALL.
    */
    const wchar_t *is_function() const;

    /** Get the "principal" parser, whatever that is */
    static parser_t &principal_parser();

    /** Indicates that execution of all blocks in the principal parser should stop.
        This is called from signal handlers!
    */
    static void skip_all_blocks();

    /** Create a parser of the given type */
    parser_t(enum parser_type_t type, bool show_errors);

    /** The current innermost block, allocated with new */
    block_t *current_block;

    /** Global event blocks */
    event_blockage_list_t global_event_blocks;

    /** Current block level io redirections  */
    io_chain_t block_io;

    /**
      Evaluate the expressions contained in cmd.

      \param cmd the string to evaluate
      \param io io redirections to perform on all started jobs
      \param block_type The type of block to push on the block stack

      \return 0 on success, 1 otherwise
    */
    int eval(const wcstring &cmdStr, const io_chain_t &io, enum block_type_t block_type);

    /**
      Evaluate line as a list of parameters, i.e. tokenize it and perform parameter expansion and cmdsubst execution on the tokens.
      The output is inserted into output, and should be freed by the caller.

      \param line Line to evaluate
      \param output List to insert output to
    */
    /**
      \param line Line to evaluate
      \param output List to insert output to
    */
    int eval_args(const wchar_t *line, std::vector<completion_t> &output);

    /**
       Sets the current evaluation error. This function should only be used by libraries that are called by

       \param ec The new error code
       \param p The character offset at which the error occured
       \param str The printf-style error message filter
    */
    void error(int ec, int p, const wchar_t *str, ...);

    /**
       Returns a string describing the current parser pisition in the format 'FILENAME (line LINE_NUMBER): LINE'.
       Example:

       init.fish (line 127): ls|grep pancake
    */
    const wchar_t *current_line();

    /** Returns the current line number */
    int get_lineno() const;

    /** Returns the line number for the character at the given index */
    int line_number_of_character_at_offset(size_t idx) const;

    /** Returns the current position in the latest string of the tokenizer. */
    int get_pos() const;

    /** Returns the position where the current job started in the latest string of the tokenizer. */
    int get_job_pos() const;

    /** Set the current position in the latest string of the tokenizer. */
    void set_pos(int p);

    /** Get the string currently parsed */
    const wchar_t *get_buffer() const;

    /** Get the list of jobs */
    job_list_t &job_list()
    {
        return my_job_list;
    }

    /** Pushes the block. pop_block will call delete on it. */
    void push_block(block_t *newv);

    /** Remove the outermost block namespace */
    void pop_block();

    /** Return a description of the given blocktype */
    const wchar_t *get_block_desc(int block) const;

    /** Removes a job */
    bool job_remove(job_t *job);

    /** Promotes a job to the front of the list */
    void job_promote(job_t *job);

    /** Return the job with the specified job id. If id is 0 or less, return the last job used. */
    job_t *job_get(int job_id);

    /** Returns the job with the given pid */
    job_t *job_get_from_pid(int pid);

    /**
       Test if the specified string can be parsed, or if more bytes need
       to be read first. The result will have the PARSER_TEST_ERROR bit
       set if there is a syntax error in the code, and the
       PARSER_TEST_INCOMPLETE bit set if the code contains unclosed
       blocks.

       \param buff the text buffer to test
       \param block_level if non-null, the block nesting level will be filled out into this array
       \param out if non-null, any errors in the command will be filled out into this buffer
       \param prefix the prefix string to prepend to each error message written to the \c out buffer
    */
    int test(const wchar_t * buff, int *block_level = NULL, wcstring *out = NULL, const wchar_t *prefix = NULL);

    /**
       Test if the specified string can be parsed as an argument list,
       e.g. sent to eval_args.  The result has the first bit set if the
       string contains errors, and the second bit is set if the string
       contains an unclosed block.
    */
    int test_args(const wchar_t * buff, wcstring *out, const wchar_t *prefix);

    /**
       Tell the parser that the specified function may not be run if not
       inside of a conditional block. This is to remove some possibilities
       of infinite recursion.
    */
    void forbid_function(const wcstring &function);
    /**
       Undo last call to parser_forbid_function().
    */
    void allow_function();

    /**
       Initialize static parser data
    */
    void init();

    /**
       Destroy static parser data
    */
    void destroy();

    /**
       This function checks if the specified string is a help option.

       \param s the string to test
       \param min_match is the minimum number of characters that must match in a long style option, i.e. the longest common prefix between --help and any other option. If less than 3, 3 will be assumed.
    */
    int is_help(const wchar_t *s, int min_match) const;

    /**
       Returns the file currently evaluated by the parser. This can be
       different than reader_current_filename, e.g. if we are evaulating a
       function defined in a different file than the one curently read.
    */
    const wchar_t *current_filename() const;

    /**
       Write a stack trace starting at the specified block to the specified wcstring
    */
    void stack_trace(block_t *b, wcstring &buff);

    int get_block_type(const wchar_t *cmd) const;
    const wchar_t *get_block_command(int type) const;
};


#endif
