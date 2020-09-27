// A specialized tokenizer for tokenizing the fish language. In the future, the tokenizer should be
// extended to support marks, tokenizing multiple strings and disposing of unused string segments.
#ifndef FISH_TOKENIZER_H
#define FISH_TOKENIZER_H

#include <stddef.h>

#include "common.h"
#include "maybe.h"
#include "parse_constants.h"
#include "redirection.h"

/// Token types.
enum class token_type_t {
    error,       /// Error reading token
    string,      /// String token
    pipe,        /// Pipe token
    andand,      /// && token
    oror,        /// || token
    end,         /// End token (semicolon or newline, not literal end)
    redirect,    /// redirection token
    background,  /// send job to bg token
    comment,     /// comment token
};

/// Flag telling the tokenizer to accept incomplete parameters, i.e. parameters with mismatching
/// parenthesis, etc. This is useful for tab-completion.
#define TOK_ACCEPT_UNFINISHED 1

/// Flag telling the tokenizer not to remove comments. Useful for syntax highlighting.
#define TOK_SHOW_COMMENTS 2

/// Ordinarily, the tokenizer ignores newlines following a newline, or a semicolon. This flag tells
/// the tokenizer to return each of them as a separate END.
#define TOK_SHOW_BLANK_LINES 4

/// Make an effort to continue after an error.
#define TOK_CONTINUE_AFTER_ERROR 8

typedef unsigned int tok_flags_t;

enum class tokenizer_error_t {
    none,
    unterminated_quote,
    unterminated_subshell,
    unterminated_slice,
    unterminated_escape,
    invalid_redirect,
    invalid_pipe,
    invalid_pipe_ampersand,
    closing_unopened_subshell,
    illegal_slice,
    closing_unopened_brace,
    unterminated_brace,
    expected_pclose_found_bclose,
    expected_bclose_found_pclose,
};

/// Get the error message for an error \p err.
const wchar_t *tokenizer_get_error_message(tokenizer_error_t err);

struct tok_t {
    // The type of the token.
    token_type_t type;

    // Offset of the token.
    size_t offset{0};
    // Length of the token.
    size_t length{0};

    // If an error, this is the error code.
    tokenizer_error_t error{tokenizer_error_t::none};

    // If an error, this is the offset of the error within the token. A value of 0 means it occurred
    // at 'offset'.
    size_t error_offset_within_token{size_t(-1)};

    // Construct from a token type.
    explicit tok_t(token_type_t type);

    /// Returns whether the given location is within the source range or at its end.
    bool location_in_or_at_end_of_source_range(size_t loc) const {
        return offset <= loc && loc - offset <= length;
    }
    /// Gets source for the token, or the empty string if it has no source.
    wcstring get_source(const wcstring &str) const { return wcstring(str, offset, length); }
};

/// The tokenizer struct.
class tokenizer_t {
    // No copying, etc.
    tokenizer_t(const tokenizer_t &) = delete;
    void operator=(const tokenizer_t &) = delete;

    /// A pointer into the original string, showing where the next token begins.
    const wchar_t *token_cursor;
    /// The start of the original string.
    const wchar_t *const start;
    /// Whether we have additional tokens.
    bool has_next{true};
    /// Whether incomplete tokens are accepted.
    bool accept_unfinished{false};
    /// Whether comments should be returned.
    bool show_comments{false};
    /// Whether all blank lines are returned.
    bool show_blank_lines{false};
    /// Whether to attempt to continue after an error.
    bool continue_after_error{false};
    /// Whether to continue the previous line after the comment.
    bool continue_line_after_comment{false};

    tok_t call_error(tokenizer_error_t error_type, const wchar_t *token_start,
                     const wchar_t *error_loc, maybe_t<size_t> token_length = {});
    tok_t read_string();

   public:
    /// Constructor for a tokenizer. b is the string that is to be tokenized. It is not copied, and
    /// should not be freed by the caller until after the tokenizer is destroyed.
    ///
    /// \param b The string to tokenize
    /// \param flags Flags to the tokenizer. Setting TOK_ACCEPT_UNFINISHED will cause the tokenizer
    /// to accept incomplete tokens, such as a subshell without a closing parenthesis, as a valid
    /// token. Setting TOK_SHOW_COMMENTS will return comments as tokens
    tokenizer_t(const wchar_t *start, tok_flags_t flags);

    /// Returns the next token, or none() if we are at the end.
    maybe_t<tok_t> next();

    /// Returns the text of a token, as a string.
    wcstring text_of(const tok_t &tok) const { return wcstring(start + tok.offset, tok.length); }

    /// Copies a token's text into a string. This is useful for reusing storage.
    /// Returns a reference to the string.
    const wcstring &copy_text_of(const tok_t &tok, wcstring *result) {
        return result->assign(start + tok.offset, tok.length);
    }
};

/// Returns only the first token from the specified string. This is a convenience function, used to
/// retrieve the first token of a string. This can be useful for error messages, etc. On failure,
/// returns the empty string.
wcstring tok_first(const wcstring &str);

/// Like to tok_first, but skip variable assignments like A=B.
wcstring tok_command(const wcstring &str);

/// Struct wrapping up a parsed pipe or redirection.
struct pipe_or_redir_t {
    // The redirected fd, or -1 on overflow.
    // In the common case of a pipe, this is 1 (STDOUT_FILENO).
    // For example, in the case of "3>&1" this will be 3.
    int fd{-1};

    // Whether we are a pipe (true) or redirection (false).
    bool is_pipe{false};

    // The redirection mode if the type is redirect.
    // Ignored for pipes.
    redirection_mode_t mode{redirection_mode_t::overwrite};

    // Whether, in addition to this redirection, stderr should also be dup'd to stdout
    // For example &| or &>
    bool stderr_merge{false};

    // Number of characters consumed when parsing the string.
    size_t consumed{0};

    // Construct from a string.
    static maybe_t<pipe_or_redir_t> from_string(const wchar_t *buff);
    static maybe_t<pipe_or_redir_t> from_string(const wcstring &buff) {
        return from_string(buff.c_str());
    }

    // \return the oflags (as in open(2)) for this redirection.
    int oflags() const;

    // \return if we are "valid". Here "valid" means only that the source fd did not overflow.
    // For example 99999999999> is invalid.
    bool is_valid() const { return fd >= 0; }

    // \return the token type for this redirection.
    token_type_t token_type() const {
        return is_pipe ? token_type_t::pipe : token_type_t::redirect;
    }

   private:
    pipe_or_redir_t();
};

enum move_word_style_t {
    move_word_style_punctuation,      // stop at punctuation
    move_word_style_path_components,  // stops at path components
    move_word_style_whitespace        // stops at whitespace
};

/// Our state machine that implements "one word" movement or erasure.
class move_word_state_machine_t {
   private:
    bool consume_char_punctuation(wchar_t c);
    bool consume_char_path_components(wchar_t c);
    bool is_path_component_character(wchar_t c);
    bool consume_char_whitespace(wchar_t c);

    int state;
    move_word_style_t style;

   public:
    explicit move_word_state_machine_t(move_word_style_t syl);
    bool consume_char(wchar_t c);
    void reset();
};

/// The position of the equal sign in a variable assignment like foo=bar.
maybe_t<size_t> variable_assignment_equals_pos(const wcstring &txt);

#endif
