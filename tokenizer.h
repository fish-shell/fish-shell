/** \file tokenizer.h

    A specialized tokenizer for tokenizing the fish language. In the
    future, the tokenizer should be extended to support marks,
    tokenizing multiple strings and disposing of unused string
    segments.
*/

#ifndef FISH_TOKENIZER_H
#define FISH_TOKENIZER_H

#include <wchar.h>
#include "common.h"

/**
   Token types
*/
enum token_type
{
    TOK_NONE, /**< Tokenizer not yet constructed */
    TOK_ERROR, /**< Error reading token */
    TOK_STRING,/**< String token */
    TOK_PIPE,/**< Pipe token */
    TOK_END,/**< End token (semicolon or newline, not literal end) */
    TOK_REDIRECT_OUT, /**< redirection token */
    TOK_REDIRECT_APPEND,/**< redirection append token */
    TOK_REDIRECT_IN,/**< input redirection token */
    TOK_REDIRECT_FD,/**< redirection to new fd token */
    TOK_REDIRECT_NOCLOB, /**<? redirection token */
    TOK_BACKGROUND,/**< send job to bg token */
    TOK_COMMENT/**< comment token */
};

/**
   Tokenizer error types
*/
enum tokenizer_error
{
    TOK_UNTERMINATED_QUOTE,
    TOK_UNTERMINATED_SUBSHELL,
    TOK_UNTERMINATED_ESCAPE,
    TOK_OTHER
}
;


/**
   Flag telling the tokenizer to accept incomplete parameters,
   i.e. parameters with mismatching paranthesis, etc. This is useful
   for tab-completion.
*/
#define TOK_ACCEPT_UNFINISHED 1

/**
   Flag telling the tokenizer not to remove comments. Useful for
   syntax highlighting.
*/
#define TOK_SHOW_COMMENTS 2

/** Flag telling the tokenizer to not generate error messages, which we need to do when tokenizing off of the main thread (since wgettext is not thread safe).
*/
#define TOK_SQUASH_ERRORS 4

/** Ordinarily, the tokenizer ignores newlines following a newline, or a semicolon.
    This flag tells the tokenizer to return each of them as a separate END. */
#define TOK_SHOW_BLANK_LINES 8

typedef unsigned int tok_flags_t;

/**
   The tokenizer struct.
*/
struct tokenizer_t
{
    /** A pointer into the original string, showing where the next token begins */
    const wchar_t *buff;
    /** A copy of the original string */
    const wchar_t *orig_buff;
    /** The last token */
    wcstring last_token;

    /** Type of last token*/
    enum token_type last_type;

    /** Offset of last token*/
    size_t last_pos;
    /** Whether there are more tokens*/
    bool has_next;
    /** Whether incomplete tokens are accepted*/
    bool accept_unfinished;
    /** Whether comments should be returned*/
    bool show_comments;
    /** Whether all blank lines are returned */
    bool show_blank_lines;
    /** Type of last quote, can be either ' or ".*/
    wchar_t last_quote;
    /** Last error */
    int error;
    /* Whether we are squashing errors */
    bool squash_errors;

    /* Cached line number information */
    size_t cached_lineno_offset;
    int cached_lineno_count;

    /* Whether to continue the previous line after the comment */
    bool continue_line_after_comment;

    /**
      Constructor for a tokenizer. b is the string that is to be
      tokenized. It is not copied, and should not be freed by the caller
      until after the tokenizer is destroyed.

      \param b The string to tokenize
      \param flags Flags to the tokenizer. Setting TOK_ACCEPT_UNFINISHED will cause the tokenizer
      to accept incomplete tokens, such as a subshell without a closing
      parenthesis, as a valid token. Setting TOK_SHOW_COMMENTS will return comments as tokens

    */
    tokenizer_t(const wchar_t *b, tok_flags_t flags);
};

/**
  Jump to the next token.
*/
void tok_next(tokenizer_t *tok);

/**
  Returns the type of the last token. Must be one of the values in the token_type enum.
*/
enum token_type tok_last_type(tokenizer_t *tok);

/**
  Returns the last token string. The string should not be freed by the caller. This returns nonsense results for some token types, like TOK_END.
*/
const wchar_t *tok_last(tokenizer_t *tok);

/**
  Returns true as long as there are more tokens left
*/
int tok_has_next(tokenizer_t *tok);

/**
  Returns the position of the beginning of the current token in the original string
*/
int tok_get_pos(const tokenizer_t *tok);

/** Returns the extent of the current token */
size_t tok_get_extent(const tokenizer_t *tok);

/**
   Returns only the first token from the specified string. This is a
   convenience function, used to retrieve the first token of a
   string. This can be useful for error messages, etc.

   On failure, returns the empty string.
*/
wcstring tok_first(const wchar_t *str);

/**
   Indicates whether a character can be part of a string, or is a string separator.
   Separators include newline, tab, |, ^, >, <, etc.

   is_first should indicate whether this is the first character in a potential string.
*/
bool tok_is_string_character(wchar_t c, bool is_first);

/**
   Move tokenizer position
*/
void tok_set_pos(tokenizer_t *tok, int pos);

/**
   Returns a string description of the specified token type
*/
const wchar_t *tok_get_desc(int type);

/**
   Get tokenizer error type. Should only be called if tok_last_tope returns TOK_ERROR.
*/
int tok_get_error(tokenizer_t *tok);

/* Helper function to determine redirection type from a string, or TOK_NONE if the redirection is invalid. Also returns the fd by reference. */
enum token_type redirection_type_for_string(const wcstring &str, int *out_fd = NULL);

/* Helper function to determine which fd is redirected by a pipe */
int fd_redirected_by_pipe(const wcstring &str);

/* Helper function to return oflags (as in open(2)) for a redirection type */
int oflags_for_redirection_type(enum token_type type);

enum move_word_style_t
{
    move_word_style_punctuation, //stop at punctuation
    move_word_style_path_components //stops at path components
};

/* Our state machine that implements "one word" movement or erasure. */
class move_word_state_machine_t
{
private:

    bool consume_char_punctuation(wchar_t c);
    bool consume_char_path_components(wchar_t c);
    bool is_path_component_character(wchar_t c);

    int state;
    move_word_style_t style;

public:

    move_word_state_machine_t(move_word_style_t st);
    bool consume_char(wchar_t c);
    void reset();
};


#endif
