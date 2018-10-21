// A specialized tokenizer for tokenizing the fish language. In the future, the tokenizer should be
// extended to support marks, tokenizing multiple strings and disposing of unused string segments.
#ifndef FISH_TOKENIZER_H
#define FISH_TOKENIZER_H

#include <stddef.h>

#include "common.h"
#include "maybe.h"
#include "parse_constants.h"

/// Token types.
enum token_type {
    TOK_NONE,        /// Tokenizer not yet constructed
    TOK_ERROR,       /// Error reading token
    TOK_STRING,      /// String token
    TOK_PIPE,        /// Pipe token
    TOK_ANDAND,      /// && token
    TOK_OROR,        /// || token
    TOK_END,         /// End token (semicolon or newline, not literal end)
    TOK_REDIRECT,    /// redirection token
    TOK_BACKGROUND,  /// send job to bg token
    TOK_COMMENT      /// comment token
};

enum class redirection_type_t {
    overwrite,  // normal redirection: > file.txt
    append,     // appending redirection: >> file.txt
    input,      // input redirection: < file.txt
    fd,         // fd redirection: 2>&1
    noclob      // noclobber redirection: >? file.txt
};

/// Flag telling the tokenizer to accept incomplete parameters, i.e. parameters with mismatching
/// paranthesis, etc. This is useful for tab-completion.
#define TOK_ACCEPT_UNFINISHED 1

/// Flag telling the tokenizer not to remove comments. Useful for syntax highlighting.
#define TOK_SHOW_COMMENTS 2

/// Ordinarily, the tokenizer ignores newlines following a newline, or a semicolon. This flag tells
/// the tokenizer to return each of them as a separate END.
#define TOK_SHOW_BLANK_LINES 4

typedef unsigned int tok_flags_t;

enum class tokenizer_error_t {
    none,
    unterminated_quote,
    unterminated_subshell,
    unterminated_slice,
    unterminated_escape,
    invalid_redirect,
    invalid_pipe,
    closing_unopened_subshell,
    illegal_slice,
    closing_unopened_brace,
    unterminated_brace,
    expected_pclose_found_bclose,
    expected_bclose_found_pclose,
};

/// Get the error message for an error \p err.
wcstring tokenizer_get_error_message(tokenizer_error_t err);

struct tok_t {
    // The type of the token.
    token_type type{TOK_NONE};

    // Offset of the token.
    size_t offset{0};
    // Length of the token.
    size_t length{0};

    // If the token represents a redirection, the redirected fd.
    maybe_t<int> redirected_fd{};

    // If an error, this is the error code.
    tokenizer_error_t error{tokenizer_error_t::none};

    // Whether the token was preceded by an escaped newline.
    bool preceding_escaped_nl{false};

    // If an error, this is the offset of the error within the token. A value of 0 means it occurred
    // at 'offset'.
    size_t error_offset{size_t(-1)};

    tok_t() = default;
};

/// The tokenizer struct.
class tokenizer_t {
    // No copying, etc.
    tokenizer_t(const tokenizer_t &) = delete;
    void operator=(const tokenizer_t &) = delete;

    /// A pointer into the original string, showing where the next token begins.
    const wchar_t *buff;
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
    /// Whether to continue the previous line after the comment.
    bool continue_line_after_comment{false};

    tok_t call_error(tokenizer_error_t error_type, const wchar_t *token_start,
                     const wchar_t *error_loc);
    tok_t read_string();
    maybe_t<tok_t> tok_next();

   public:
    /// Constructor for a tokenizer. b is the string that is to be tokenized. It is not copied, and
    /// should not be freed by the caller until after the tokenizer is destroyed.
    ///
    /// \param b The string to tokenize
    /// \param flags Flags to the tokenizer. Setting TOK_ACCEPT_UNFINISHED will cause the tokenizer
    /// to accept incomplete tokens, such as a subshell without a closing parenthesis, as a valid
    /// token. Setting TOK_SHOW_COMMENTS will return comments as tokens
    tokenizer_t(const wchar_t *b, tok_flags_t flags);

    /// Returns the next token by reference. Returns true if we got one, false if we're at the end.
    bool next(struct tok_t *result);

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

/// Helper function to determine redirection type from a string, or TOK_NONE if the redirection is
/// invalid. Also returns the fd by reference.
maybe_t<redirection_type_t> redirection_type_for_string(const wcstring &str, int *out_fd = NULL);

/// Helper function to determine which fd is redirected by a pipe.
int fd_redirected_by_pipe(const wcstring &str);

/// Helper function to return oflags (as in open(2)) for a redirection type.
int oflags_for_redirection_type(redirection_type_t type);

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
    explicit move_word_state_machine_t(move_word_style_t st);
    bool consume_char(wchar_t c);
    void reset();
};

#endif
