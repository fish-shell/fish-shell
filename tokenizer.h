/** \file tokenizer.h 

    A specialized tokenizer for tokenizing the fish language. In the
    future, the tokenizer should be extended to support marks,
    tokenizing multiple strings and disposing of unused string
    segments.
*/

#ifndef FISH_TOKENIZER_H
#define FISH_TOKENIZER_H

#include <wchar.h>

/**
   Token types
*/
enum token_type
{
	TOK_NONE, /**< Tokenizer not yet constructed */
	TOK_ERROR, /**< Error reading token */
	TOK_INVALID,/**< Invalid token */
	TOK_STRING,/**< String token */
	TOK_PIPE,/**< Pipe token */
	TOK_END,/**< End token */
	TOK_REDIRECT_OUT, /**< redirection token */
	TOK_REDIRECT_APPEND,/**< redirection append token */
	TOK_REDIRECT_IN,/**< input redirection token */
	TOK_REDIRECT_FD,/**< redirection to new fd token */
	TOK_REDIRECT_NOCLOB, /**<? redirection token */
	TOK_BACKGROUND,/**< send job to bg token */
	TOK_COMMENT/**< comment token */
}
	;

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


/**
   The tokenizer struct. 
*/
typedef struct
{
	/** A pointer into the original string, showing where the next token begins */
	wchar_t *buff;
	/** A copy of the original string */
	wchar_t *orig_buff;
	/** A pointer to the last token*/
	wchar_t *last;
	
	/** Type of last token*/
	int last_type;
	/** Length of last token*/
	int last_len;
	/** Offset of last token*/
	int last_pos;
	/** Whether there are more tokens*/
	int has_next;
	/** Whether incomplete tokens are accepted*/
	int accept_unfinished;
	/** Whether commants should be returned*/
	int show_comments;
	/** Flag set to true of the orig_buff points to an internal string that needs to be free()d when deallocating the tokenizer. */
	int free_orig;
	/** Type of last quote, can be either ' or ".*/
	wchar_t last_quote;
	/** Last error */
	int error;
}
tokenizer;

/**
  Initialize the tokenizer. b is the string that is to be
  tokenized. It is not copied, and should not be freed by the caller
  until after the tokenizer is destroyed.

  \param tok The tokenizer to initialize
  \param b The string to tokenize
  \param flags Flags to the tokenizer. Setting TOK_ACCEPT_UNFINISHED will cause the tokenizer
  to accept incomplete tokens, such as a subshell without a closing
  parenthesis, as a valid token. Setting TOK_SHOW_COMMENTS will return comments as tokens
  
*/
void tok_init( tokenizer *tok, const wchar_t *b, int flags );

/**
  Jump to the next token.
*/
void tok_next( tokenizer *tok );

/**
  Returns the type of the last token. Must be one of the values in the token_type enum.
*/
int tok_last_type( tokenizer *tok );

/**
  Returns the last token string. The string should not be freed by the caller.
*/
wchar_t *tok_last( tokenizer *tok );

/**
  Returns the type of quote from the last TOK_QSTRING
*/
wchar_t tok_last_quote( tokenizer *tok );

/**
  Returns true as long as there are more tokens left
*/
int tok_has_next( tokenizer *tok );

/**
  Returns the position of the beginning of the current token in the original string
*/
int tok_get_pos( tokenizer *tok );

/**
   Destroy the tokenizer and free asociated memory
*/
void tok_destroy( tokenizer *tok );


/**
   Returns the original string to tokenizer
 */
wchar_t *tok_string( tokenizer *tok );


/**
   Returns only the first token from the specified string. This is a
   convenience function, used to retrieve the first token of a
   string. This can be useful for error messages, etc.

   The string should be freed. After use.
*/
wchar_t *tok_first( const wchar_t *str );

/**
   Move tokenizer position
*/
void tok_set_pos( tokenizer *tok, int pos );

/**
   Returns a string description of the specified token type
*/
const wchar_t *tok_get_desc( int type );

/**
   Get tokenizer error type. Should only be called if tok_last_tope returns TOK_ERROR.
*/
int tok_get_error( tokenizer *tok );


#endif
