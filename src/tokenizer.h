/** \file tokenizer.h

    A specialized tokenizer for tokenizing the fish language. In the
    future, the tokenizer should be extended to support marks,
    tokenizing multiple strings and disposing of unused string
    segments.
*/
#ifndef FISH_TOKENIZER_H
#define FISH_TOKENIZER_H

#include <stddef.h>
#include <stdbool.h>

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
    TOK_ERROR_NONE,
    TOK_UNTERMINATED_QUOTE,
    TOK_UNTERMINATED_SUBSHELL,
    TOK_UNTERMINATED_SLICE,
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

struct tok_t
{
    /* The text of the token, or an error message for type error */
    wcstring text;
    
    /* The type of the token */
    token_type type;
    
    /* If an error, this is the error code */
    enum tokenizer_error error;
    
    /* If an error, this is the offset of the error within the token. A value of 0 means it occurred at 'offset' */
    size_t error_offset;
    
    /* Offset of the token */
    size_t offset;
    
    /* Length of the token */
    size_t length;
    
    tok_t() : type(TOK_NONE), error(TOK_ERROR_NONE), error_offset(-1), offset(-1), length(-1) {}
};

/**
   The tokenizer struct.
*/
class tokenizer_t
{
    /* No copying, etc. */
    tokenizer_t(const tokenizer_t&);
    void operator=(const tokenizer_t&);

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
    /** Last error */
    tokenizer_error error;
    /** Last error offset, in "global" coordinates (relative to orig_buff) */
    size_t global_error_offset;
    /* Whether we are squashing errors */
    bool squash_errors;

    /* Whether to continue the previous line after the comment */
    bool continue_line_after_comment;
    
    void call_error(enum tokenizer_error error_type, const wchar_t *where, const wchar_t *error_message);
    void read_string();
    void read_comment();
    void tok_next();
    
public:
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
    
    /** Returns the next token by reference. Returns true if we got one, false if we're at the end. */
    bool next(struct tok_t *result);
};


/**
   Returns only the first token from the specified string. This is a
   convenience function, used to retrieve the first token of a
   string. This can be useful for error messages, etc.

   On failure, returns the empty string.
*/
wcstring tok_first(const wcstring &str);

/* Helper function to determine redirection type from a string, or TOK_NONE if the redirection is invalid. Also returns the fd by reference. */
enum token_type redirection_type_for_string(const wcstring &str, int *out_fd = NULL);

/* Helper function to determine which fd is redirected by a pipe */
int fd_redirected_by_pipe(const wcstring &str);

/* Helper function to return oflags (as in open(2)) for a redirection type */
int oflags_for_redirection_type(enum token_type type);

enum move_word_style_t
{
    move_word_style_punctuation, //stop at punctuation
    move_word_style_path_components, //stops at path components
    move_word_style_whitespace // stops at whitespace
};

/* Our state machine that implements "one word" movement or erasure. */
class move_word_state_machine_t
{
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
