// Various mostly unrelated utility functions related to parsing, loading and evaluating fish code.
#ifndef FISH_PARSE_UTIL_H
#define FISH_PARSE_UTIL_H

#include <stddef.h>

#include <vector>

#include "ast.h"
#include "common.h"
#include "cxx.h"
#include "maybe.h"
#include "parse_constants.h"
#if INCLUDE_RUST_HEADERS
#include "parse_util.rs.h"
#endif

struct Tok;
using tok_t = Tok;

/// Handles slices: the square brackets in an expression like $foo[5..4]
/// \return the length of the slice starting at \p in, or 0 if there is no slice, or -1 on error.
/// This never accepts incomplete slices.
long parse_util_slice_length(const wchar_t *in);

/// Alternative API. Iterate over command substitutions.
///
/// \param str the string to search for subshells
/// \param inout_cursor_offset On input, the location to begin the search. On output, either the end
/// of the string, or just after the closed-paren.
/// \param out_contents On output, the contents of the command substitution
/// \param out_start On output, the offset of the start of the command substitution (open paren)
/// \param out_end On output, the offset of the end of the command substitution (close paren), or
/// the end of the string if it was incomplete
/// \param accept_incomplete whether to permit missing closing parenthesis
/// \param inout_is_quoted whether the cursor is in a double-quoted context.
/// \param out_has_dollar whether the command substitution has the optional leading $.
/// \return -1 on syntax error, 0 if no subshells exist and 1 on success
int parse_util_locate_cmdsubst_range(const wcstring &str, size_t *inout_cursor_offset,
                                     wcstring *out_contents, size_t *out_start, size_t *out_end,
                                     bool accept_incomplete, bool *inout_is_quoted = nullptr,
                                     bool *out_has_dollar = nullptr);

/// Find the beginning and end of the command substitution under the cursor. If no subshell is
/// found, the entire string is returned. If the current command substitution is not ended, i.e. the
/// closing parenthesis is missing, then the string from the beginning of the substitution to the
/// end of the string is returned.
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
/// \param a the start of the searched string
/// \param b the end of the searched string
void parse_util_cmdsubst_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **a,
                                const wchar_t **b);

/// Find the beginning and end of the process definition under the cursor
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
/// \param a the start of the process
/// \param b the end of the process
/// \param tokens the tokens in the process
void parse_util_process_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **a,
                               const wchar_t **b, std::vector<tok_t> *tokens);

/// Find the beginning and end of the job definition under the cursor
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
/// \param a the start of the searched string
/// \param b the end of the searched string
void parse_util_job_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **a,
                           const wchar_t **b);

/// Find the beginning and end of the token under the cursor and the token before the current token.
/// Any combination of tok_begin, tok_end, prev_begin and prev_end may be null.
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
/// \param tok_begin the start of the current token
/// \param tok_end the end of the current token
/// \param prev_begin the start o the token before the current token
/// \param prev_end the end of the token before the current token
void parse_util_token_extent(const wchar_t *buff, size_t cursor_pos, const wchar_t **tok_begin,
                             const wchar_t **tok_end, const wchar_t **prev_begin,
                             const wchar_t **prev_end);

/// Get the line number at the specified character offset.
int parse_util_lineno(const wcstring &str, size_t offset);

/// Calculate the line number of the specified cursor position.
int parse_util_get_line_from_offset(const wcstring &str, size_t pos);

/// Get the offset of the first character on the specified line.
size_t parse_util_get_offset_from_line(const wcstring &str, int line);

/// Return the total offset of the buffer for the cursor position nearest to the specified position.
size_t parse_util_get_offset(const wcstring &str, int line, long line_offset);

/// Return the given string, unescaping wildcard characters but not performing any other character
/// transformation.
wcstring parse_util_unescape_wildcards(const wcstring &str);

/// Checks if the specified string is a help option.
bool parse_util_argument_is_help(const wcstring &s);

/// Calculates information on the parameter at the specified index.
///
/// \param cmd The command to be analyzed
/// \param pos An index in the string which is inside the parameter
/// \return the type of quote used by the parameter: either ' or " or \0.
wchar_t parse_util_get_quote_type(const wcstring &cmd, size_t pos);

/// Attempts to escape the string 'cmd' using the given quote type, as determined by the quote
/// character. The quote can be a single quote or double quote, or L'\0' to indicate no quoting (and
/// thus escaping should be with backslashes). Optionally do not escape tildes.
wcstring parse_util_escape_string_with_quote(const wcstring &cmd, wchar_t quote,
                                             bool no_tilde = false);

/// Given a string, parse it as fish code and then return the indents. The return value has the same
/// size as the string.
std::vector<int> parse_util_compute_indents(const wcstring &src);

/// Given a string, detect parse errors in it. If allow_incomplete is set, then if the string is
/// incomplete (e.g. an unclosed quote), an error is not returned and the PARSER_TEST_INCOMPLETE bit
/// is set in the return value. If allow_incomplete is not set, then incomplete strings result in an
/// error.
parser_test_error_bits_t parse_util_detect_errors(const wcstring &buff_src,
                                                  parse_error_list_t *out_errors = nullptr,
                                                  bool allow_incomplete = false);

parser_test_error_bits_t parse_util_detect_errors_ffi(const ast::ast_t *ast,
                                                      const wcstring &buff_src,
                                                      parse_error_list_t *out_errors);
/// Like parse_util_detect_errors but accepts an already-parsed ast.
/// The top of the ast is assumed to be a job list.
parser_test_error_bits_t parse_util_detect_errors(const ast::ast_t &ast, const wcstring &buff_src,
                                                  parse_error_list_t *out_errors);

/// Detect errors in the specified string when parsed as an argument list. Returns the text of an
/// error, or none if no error occurred.
maybe_t<wcstring> parse_util_detect_errors_in_argument_list(const wcstring &arg_list_src,
                                                            const wcstring &prefix = {});

/// Test if this argument contains any errors. Detected errors include syntax errors in command
/// substitutions, improperly escaped characters and improper use of the variable expansion
/// operator. This does NOT currently detect unterminated quotes.

parser_test_error_bits_t parse_util_detect_errors_in_argument(
    const ast::argument_t &arg, const wcstring &arg_src, parse_error_list_t *out_errors = nullptr);

/// Given a string containing a variable expansion error, append an appropriate error to the errors
/// list. The global_token_pos is the offset of the token in the larger source, and the dollar_pos
/// is the offset of the offending dollar sign within the token.
void parse_util_expand_variable_error(const wcstring &token, size_t global_token_pos,
                                      size_t dollar_pos, parse_error_list_t *out_errors);

#endif
