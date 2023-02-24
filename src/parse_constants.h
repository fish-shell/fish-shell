// Constants used in the programmatic representation of fish code.
#ifndef FISH_PARSE_CONSTANTS_H
#define FISH_PARSE_CONSTANTS_H

#include "common.h"

using source_offset_t = uint32_t;
constexpr source_offset_t SOURCE_OFFSET_INVALID = static_cast<source_offset_t>(-1);

#define PARSER_DIE()                   \
    do {                               \
        FLOG(error, L"Parser dying!"); \
        exit_without_destructors(-1);  \
    } while (0)

#if INCLUDE_RUST_HEADERS

#include "parse_constants.rs.h"

using source_range_t = SourceRange;
using parse_token_type_t = ParseTokenType;
using parse_keyword_t = ParseKeyword;
using statement_decoration_t = StatementDecoration;
using parse_error_code_t = ParseErrorCode;
using pipeline_position_t = PipelinePosition;
using parse_error_list_t = ParseErrorList;

#else

// Hacks to allow us to compile without Rust headers.

#include "config.h"

struct SourceRange {
    source_offset_t start;
    source_offset_t length;
};
using source_range_t = SourceRange;

enum class parse_token_type_t : uint8_t {
    invalid = 1,
    string,
    pipe,
    redirection,
    background,
    andand,
    oror,
    end,
    terminate,
    error,
    tokenizer_error,
    comment,
};

enum class parse_keyword_t : uint8_t {
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

enum class statement_decoration_t : uint8_t {
    none,
    command,
    builtin,
    exec,
};

enum class parse_error_code_t : uint8_t {
    none,
    syntax,
    cmdsubst,
    generic,
    tokenizer_unterminated_quote,
    tokenizer_unterminated_subshell,
    tokenizer_unterminated_slice,
    tokenizer_unterminated_escape,
    tokenizer_other,
    unbalancing_end,
    unbalancing_else,
    unbalancing_case,
    bare_variable_assignment,
    andor_in_pipeline,
};

struct ParseErrorList;
using parse_error_list_t = ParseErrorList;

#endif

// Special source_start value that means unknown.
#define SOURCE_LOCATION_UNKNOWN (static_cast<size_t>(-1))

enum {
    parse_flag_none = 0,
    parse_flag_continue_after_error = 1 << 0,
    parse_flag_include_comments = 1 << 1,
    parse_flag_accept_incomplete_tokens = 1 << 2,
    parse_flag_leave_unterminated = 1 << 3,
    parse_flag_show_blank_lines = 1 << 4,
    parse_flag_show_extra_semis = 1 << 5,
};
using parse_tree_flags_t = uint8_t;

enum { PARSER_TEST_ERROR = 1, PARSER_TEST_INCOMPLETE = 2 };
using parser_test_error_bits_t = uint8_t;

/// Maximum number of function calls.
#define FISH_MAX_STACK_DEPTH 128

/// Maximum number of nested string substitutions (in lieu of evals)
/// Reduced under TSAN: our CI test creates 500 jobs and this is very slow with TSAN.
#if FISH_TSAN_WORKAROUNDS
#define FISH_MAX_EVAL_DEPTH 250
#else
#define FISH_MAX_EVAL_DEPTH 500
#endif

/// Error message on a function that calls itself immediately.
#define INFINITE_FUNC_RECURSION_ERR_MSG \
    _(L"The function '%ls' calls itself immediately, which would result in an infinite loop.")

/// Error message on reaching maximum call stack depth.
#define CALL_STACK_LIMIT_EXCEEDED_ERR_MSG \
    _(L"The call stack limit has been exceeded. Do you have an accidental infinite loop?")

/// Error message when encountering an unknown builtin name.
#define UNKNOWN_BUILTIN_ERR_MSG _(L"Unknown builtin '%ls'")

/// Error message when encountering a failed expansion, e.g. for the variable name in for loops.
#define FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG _(L"Unable to expand variable name '%ls'")

/// Error message when encountering an illegal file descriptor.
#define ILLEGAL_FD_ERR_MSG _(L"Illegal file descriptor in redirection '%ls'")

/// Error message for wildcards with no matches.
#define WILDCARD_ERR_MSG _(L"No matches for wildcard '%ls'. See `help wildcards-globbing`.")

/// Error when using break outside of loop.
#define INVALID_BREAK_ERR_MSG _(L"'break' while not inside of loop")

/// Error when using continue outside of loop.
#define INVALID_CONTINUE_ERR_MSG _(L"'continue' while not inside of loop")

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

/// Error issued on { echo; echo }.
#define ERROR_NO_BRACE_GROUPING \
    _(L"'{ ... }' is not supported for grouping commands. Please use 'begin; ...; end'")

#endif
