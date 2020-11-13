// Constants used in the programmatic representation of fish code.
#ifndef FISH_PARSE_CONSTANTS_H
#define FISH_PARSE_CONSTANTS_H

#include "config.h"

#include "common.h"

#define PARSER_DIE()                   \
    do {                               \
        FLOG(error, L"Parser dying!"); \
        exit_without_destructors(-1);  \
    } while (0)

// A range of source code.
struct source_range_t {
    uint32_t start;
    uint32_t length;

    uint32_t end() const {
        assert(start + length >= start && "Overflow");
        return start + length;
    }
};

// IMPORTANT: If the following enum table is modified you must also update token_enum_map below.
enum class parse_token_type_t : uint8_t {
    invalid = 1,

    // Terminal types.
    string,
    pipe,
    redirection,
    background,
    andand,
    oror,
    end,
    // Special terminal type that means no more tokens forthcoming.
    terminate,
    // Very special terminal types that don't appear in the production list.
    error,
    tokenizer_error,
    comment,
};

const enum_map<parse_token_type_t> token_enum_map[] = {
    {parse_token_type_t::comment, L"parse_token_type_t::comment"},
    {parse_token_type_t::error, L"parse_token_type_t::error"},
    {parse_token_type_t::tokenizer_error, L"parse_token_type_t::tokenizer_error"},
    {parse_token_type_t::background, L"parse_token_type_t::background"},
    {parse_token_type_t::end, L"parse_token_type_t::end"},
    {parse_token_type_t::pipe, L"parse_token_type_t::pipe"},
    {parse_token_type_t::redirection, L"parse_token_type_t::redirection"},
    {parse_token_type_t::string, L"parse_token_type_t::string"},
    {parse_token_type_t::andand, L"parse_token_type_t::andand"},
    {parse_token_type_t::oror, L"parse_token_type_t::oror"},
    {parse_token_type_t::terminate, L"parse_token_type_t::terminate"},
    {parse_token_type_t::invalid, L"parse_token_type_t::invalid"},
    {parse_token_type_t::invalid, nullptr}};
#define token_enum_map_len (sizeof token_enum_map / sizeof *token_enum_map)

// IMPORTANT: If the following enum is modified you must update the corresponding keyword_enum_map
// array below.
//
// IMPORTANT: These enums must start at zero.
enum class parse_keyword_t : uint8_t {
    // 'none' is not a keyword, it is a sentinel indicating nothing.
    none,

    kw_and,
    kw_begin,
    kw_builtin,
    kw_case,
    kw_command,
    kw_else,
    kw_end,
    kw_exclam,
    kw_exec,
    kw_for,
    kw_function,
    kw_if,
    kw_in,
    kw_not,
    kw_or,
    kw_switch,
    kw_time,
    kw_while,
};

const enum_map<parse_keyword_t> keyword_enum_map[] = {{parse_keyword_t::kw_exclam, L"!"},
                                                      {parse_keyword_t::kw_and, L"and"},
                                                      {parse_keyword_t::kw_begin, L"begin"},
                                                      {parse_keyword_t::kw_builtin, L"builtin"},
                                                      {parse_keyword_t::kw_case, L"case"},
                                                      {parse_keyword_t::kw_command, L"command"},
                                                      {parse_keyword_t::kw_else, L"else"},
                                                      {parse_keyword_t::kw_end, L"end"},
                                                      {parse_keyword_t::kw_exec, L"exec"},
                                                      {parse_keyword_t::kw_for, L"for"},
                                                      {parse_keyword_t::kw_function, L"function"},
                                                      {parse_keyword_t::kw_if, L"if"},
                                                      {parse_keyword_t::kw_in, L"in"},
                                                      {parse_keyword_t::kw_not, L"not"},
                                                      {parse_keyword_t::kw_or, L"or"},
                                                      {parse_keyword_t::kw_switch, L"switch"},
                                                      {parse_keyword_t::kw_time, L"time"},
                                                      {parse_keyword_t::kw_while, L"while"},
                                                      {parse_keyword_t::none, nullptr}};
#define keyword_enum_map_len (sizeof keyword_enum_map / sizeof *keyword_enum_map)

// Statement decorations like 'command' or 'exec'.
enum class statement_decoration_t {
    none,
    command,
    builtin,
    exec,
};

// Parse error code list.
enum parse_error_code_t {
    parse_error_none,

    // Matching values from enum parser_error.
    parse_error_syntax,
    parse_error_eval,
    parse_error_cmdsubst,

    parse_error_generic,  // unclassified error types

    // Tokenizer errors.
    parse_error_tokenizer_unterminated_quote,
    parse_error_tokenizer_unterminated_subshell,
    parse_error_tokenizer_unterminated_slice,
    parse_error_tokenizer_unterminated_escape,
    parse_error_tokenizer_other,

    parse_error_unbalancing_end,           // end outside of block
    parse_error_unbalancing_else,          // else outside of if
    parse_error_unbalancing_case,          // case outside of switch
    parse_error_bare_variable_assignment,  // a=b without command
    parse_error_andor_in_pipeline,         // "and" or "or" after a pipe
};

enum {
    parse_flag_none = 0,

    /// Attempt to build a "parse tree" no matter what. This may result in a 'forest' of
    /// disconnected trees. This is intended to be used by syntax highlighting.
    parse_flag_continue_after_error = 1 << 0,
    /// Include comment tokens.
    parse_flag_include_comments = 1 << 1,
    /// Indicate that the tokenizer should accept incomplete tokens */
    parse_flag_accept_incomplete_tokens = 1 << 2,
    /// Indicate that the parser should not generate the terminate token, allowing an 'unfinished'
    /// tree where some nodes may have no productions.
    parse_flag_leave_unterminated = 1 << 3,
    /// Indicate that the parser should generate job_list entries for blank lines.
    parse_flag_show_blank_lines = 1 << 4,
    /// Indicate that extra semis should be generated.
    parse_flag_show_extra_semis = 1 << 5,
};
typedef unsigned int parse_tree_flags_t;

enum { PARSER_TEST_ERROR = 1, PARSER_TEST_INCOMPLETE = 2 };
typedef unsigned int parser_test_error_bits_t;

struct parse_error_t {
    /// Text of the error.
    wcstring text;
    /// Code for the error.
    enum parse_error_code_t code;
    /// Offset and length of the token in the source code that triggered this error.
    size_t source_start;
    size_t source_length;
    /// Return a string describing the error, suitable for presentation to the user. If
    /// is_interactive is true, the offending line with a caret is printed as well.
    wcstring describe(const wcstring &src, bool is_interactive) const;
    /// Return a string describing the error, suitable for presentation to the user, with the given
    /// prefix. If skip_caret is false, the offending line with a caret is printed as well.
    wcstring describe_with_prefix(const wcstring &src, const wcstring &prefix, bool is_interactive,
                                  bool skip_caret) const;
};
typedef std::vector<parse_error_t> parse_error_list_t;

wcstring token_type_user_presentable_description(parse_token_type_t type,
                                                 parse_keyword_t keyword = parse_keyword_t::none);

// Special source_start value that means unknown.
#define SOURCE_LOCATION_UNKNOWN (static_cast<size_t>(-1))

/// Helper function to offset error positions by the given amount. This is used when determining
/// errors in a substring of a larger source buffer.
void parse_error_offset_source_start(parse_error_list_t *errors, size_t amt);

// The location of a pipeline.
enum class pipeline_position_t {
    none,       // not part of a pipeline
    first,      // first command in a pipeline
    subsequent  // second or further command in a pipeline
};

/// Maximum number of function calls.
#define FISH_MAX_STACK_DEPTH 128

/// Error message on a function that calls itself immediately.
#define INFINITE_FUNC_RECURSION_ERR_MSG \
    _(L"The function '%ls' calls itself immediately, which would result in an infinite loop.")

/// Error message on reaching maximum call stack depth.
#define CALL_STACK_LIMIT_EXCEEDED_ERR_MSG                                                     \
    _(L"The function call stack limit has been exceeded. Do you have an accidental infinite loop?")

/// Error message when encountering an illegal command name.
#define ILLEGAL_CMD_ERR_MSG _(L"Illegal command name '%ls'")

/// Error message when encountering an unknown builtin name.
#define UNKNOWN_BUILTIN_ERR_MSG _(L"Unknown builtin '%ls'")

/// Error message when encountering a failed expansion, e.g. for the variable name in for loops.
#define FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG _(L"Unable to expand variable name '%ls'")

/// Error message when encountering an illegal file descriptor.
#define ILLEGAL_FD_ERR_MSG _(L"Illegal file descriptor in redirection '%ls'")

/// Error message for wildcards with no matches.
#define WILDCARD_ERR_MSG _(L"No matches for wildcard '%ls'. See `help expand`.")

/// Error when using break outside of loop.
#define INVALID_BREAK_ERR_MSG _(L"'break' while not inside of loop")

/// Error when using continue outside of loop.
#define INVALID_CONTINUE_ERR_MSG _(L"'continue' while not inside of loop")

/// Error when using return builtin outside of function definition.
#define INVALID_RETURN_ERR_MSG _(L"'return' outside of function definition")

// Error messages. The number is a reminder of how many format specifiers are contained.

/// Error for $^.
#define ERROR_BAD_VAR_CHAR1 _(L"$%lc is not a valid variable in fish.")

/// Error for ${a}.
#define ERROR_BRACKETED_VARIABLE1 _(L"Variables cannot be bracketed. In fish, please use {$%ls}.")

/// Error for "${a}".
#define ERROR_BRACKETED_VARIABLE_QUOTED1 \
    _(L"Variables cannot be bracketed. In fish, please use \"$%ls\".")

/// Error issued on $?.
#define ERROR_NOT_STATUS _(L"$? is not the exit status. In fish, please use $status.")

/// Error issued on $$.
#define ERROR_NOT_PID _(L"$$ is not the pid. In fish, please use $fish_pid.")

/// Error issued on $#.
#define ERROR_NOT_ARGV_COUNT _(L"$# is not supported. In fish, please use 'count $argv'.")

/// Error issued on $@.
#define ERROR_NOT_ARGV_AT _(L"$@ is not supported. In fish, please use $argv.")

/// Error issued on $(...).
#define ERROR_BAD_VAR_SUBCOMMAND1 _(L"$(...) is not supported. In fish, please use '(%ls)'.")

/// Error issued on $*.
#define ERROR_NOT_ARGV_STAR _(L"$* is not supported. In fish, please use $argv.")

/// Error issued on $.
#define ERROR_NO_VAR_NAME _(L"Expected a variable name after this $.")

/// Error message for Posix-style assignment: foo=bar.
#define ERROR_BAD_COMMAND_ASSIGN_ERR_MSG \
    _(L"Unsupported use of '='. In fish, please use 'set %ls %ls'.")

/// Error message for a command like `time foo &`.
#define ERROR_TIME_BACKGROUND \
    _(L"'time' is not supported for background jobs. Consider using 'command time'.")

#endif
