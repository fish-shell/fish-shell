/** \file tokenizer.c

A specialized tokenizer for tokenizing the fish language. In the
future, the tokenizer should be extended to support marks,
tokenizing multiple strings and disposing of unused string
segments.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "tokenizer.h"
#include "common.h"

/* Wow what a hack */
#define TOK_CALL_ERROR(t, e, x) do { tok_call_error((t), (e), (t)->squash_errors ? L"" : (x)); } while (0)

/**
   Error string for unexpected end of string
*/
#define QUOTE_ERROR _( L"Unexpected end of string, quotes are not balanced" )

/**
   Error string for mismatched parenthesis
*/
#define PARAN_ERROR _( L"Unexpected end of string, parenthesis do not match" )

/**
   Error string for mismatched square brackets
*/
#define SQUARE_BRACKET_ERROR _( L"Unexpected end of string, square brackets do not match" )


/**
   Error string for invalid redirections
*/
#define REDIRECT_ERROR _( L"Invalid input/output redirection" )

/**
   Error string for when trying to pipe from fd 0
*/
#define PIPE_ERROR _( L"Cannot use stdin (fd 0) as pipe output" )

/**
   Characters that separate tokens. They are ordered by frequency of occurrence to increase parsing speed.
*/
#define SEP L" \n|\t;#\r<>^&"

/**
   Descriptions of all tokenizer errors
*/
static const wchar_t *tok_desc[] =
{
    N_(L"Tokenizer not yet initialized"),
    N_(L"Tokenizer error"),
    N_(L"String"),
    N_(L"Pipe"),
    N_(L"End of command"),
    N_(L"Redirect output to file"),
    N_(L"Append output to file"),
    N_(L"Redirect input to file"),
    N_(L"Redirect to file descriptor"),
    N_(L"Redirect output to file if file does not exist"),
    N_(L"Run job in background"),
    N_(L"Comment")
};



/**
   Set the latest tokens string to be the specified error message
*/
static void tok_call_error(tokenizer_t *tok, int error_type, const wchar_t *error_message)
{
    tok->last_type = TOK_ERROR;
    tok->error = error_type;
    tok->last_token = error_message;
}

int tok_get_error(tokenizer_t *tok)
{
    return tok->error;
}


tokenizer_t::tokenizer_t(const wchar_t *b, tok_flags_t flags) : buff(NULL), orig_buff(NULL), last_type(TOK_NONE), last_pos(0), has_next(false), accept_unfinished(false), show_comments(false), last_quote(0), error(0), squash_errors(false), cached_lineno_offset(0), cached_lineno_count(0), continue_line_after_comment(false)
{
    CHECK(b,);

    this->accept_unfinished = !!(flags & TOK_ACCEPT_UNFINISHED);
    this->show_comments = !!(flags & TOK_SHOW_COMMENTS);
    this->squash_errors = !!(flags & TOK_SQUASH_ERRORS);
    this->show_blank_lines = !!(flags & TOK_SHOW_BLANK_LINES);

    this->has_next = (*b != L'\0');
    this->orig_buff = this->buff = b;
    this->cached_lineno_offset = 0;
    this->cached_lineno_count = 0;
    tok_next(this);
}

enum token_type tok_last_type(tokenizer_t *tok)
{
    CHECK(tok, TOK_ERROR);
    CHECK(tok->buff, TOK_ERROR);

    return tok->last_type;
}

const wchar_t *tok_last(tokenizer_t *tok)
{
    CHECK(tok, 0);

    return tok->last_token.c_str();
}

int tok_has_next(tokenizer_t *tok)
{
    /*
      Return 1 on broken tokenizer
    */
    CHECK(tok, 1);
    CHECK(tok->buff, 1);

    /*  fwprintf( stderr, L"has_next is %ls \n", tok->has_next?L"true":L"false" );*/
    return   tok->has_next;
}

/**
   Tests if this character can be a part of a string. The redirect ^ is allowed unless it's the first character.
   Hash (#) starts a comment if it's the first character in a token; otherwise it is considered a string character.
   See #953.
*/
bool tok_is_string_character(wchar_t c, bool is_first)
{
    switch (c)
    {
            /* Unconditional separators */
        case L'\0':
        case L' ':
        case L'\n':
        case L'|':
        case L'\t':
        case L';':
        case L'\r':
        case L'<':
        case L'>':
        case L'&':
            return false;

            /* Conditional separator */
        case L'^':
            return ! is_first;

        default:
            return true;
    }
}

/**
   Quick test to catch the most common 'non-magical' characters, makes
   read_string slightly faster by adding a fast path for the most
   common characters. This is obviously not a suitable replacement for
   iswalpha.
*/
static int myal(wchar_t c)
{
    return (c>=L'a' && c<=L'z') || (c>=L'A'&&c<=L'Z');
}

/**
   Read the next token as a string
*/
static void read_string(tokenizer_t *tok)
{
    const wchar_t *start;
    long len;
    int do_loop=1;
    int paran_count=0;

    start = tok->buff;
    bool is_first = true;

    enum tok_mode_t
    {
        mode_regular_text = 0, // regular text
        mode_subshell = 1, // inside of subshell
        mode_array_brackets = 2, // inside of array brackets
        mode_array_brackets_and_subshell = 3 // inside of array brackets and subshell, like in '$foo[(ech'
    } mode = mode_regular_text;

    while (1)
    {
        if (!myal(*tok->buff))
        {
            if (*tok->buff == L'\\')
            {
                tok->buff++;
                if (*tok->buff == L'\0')
                {
                    if ((!tok->accept_unfinished))
                    {
                        TOK_CALL_ERROR(tok, TOK_UNTERMINATED_ESCAPE, QUOTE_ERROR);
                        return;
                    }
                    else
                    {
                        /* Since we are about to increment tok->buff, decrement it first so the increment doesn't go past the end of the buffer. https://github.com/fish-shell/fish-shell/issues/389 */
                        tok->buff--;
                        do_loop = 0;
                    }
                }

                tok->buff++;
                continue;
            }

            switch (mode)
            {
                case mode_regular_text:
                {
                    switch (*tok->buff)
                    {
                        case L'(':
                        {
                            paran_count=1;
                            mode = mode_subshell;
                            break;
                        }

                        case L'[':
                        {
                            if (tok->buff != start)
                                mode = mode_array_brackets;
                            break;
                        }

                        case L'\'':
                        case L'"':
                        {

                            const wchar_t *end = quote_end(tok->buff);
                            tok->last_quote = *tok->buff;
                            if (end)
                            {
                                tok->buff=(wchar_t *)end;
                            }
                            else
                            {
                                tok->buff += wcslen(tok->buff);

                                if (! tok->accept_unfinished)
                                {
                                    TOK_CALL_ERROR(tok, TOK_UNTERMINATED_QUOTE, QUOTE_ERROR);
                                    return;
                                }
                                do_loop = 0;

                            }
                            break;
                        }

                        default:
                        {
                            if (! tok_is_string_character(*(tok->buff), is_first))
                            {
                                do_loop=0;
                            }
                        }
                    }
                    break;
                }

                case mode_array_brackets_and_subshell:
                case mode_subshell:
                    switch (*tok->buff)
                    {
                        case L'\'':
                        case L'\"':
                        {
                            const wchar_t *end = quote_end(tok->buff);
                            if (end)
                            {
                                tok->buff=(wchar_t *)end;
                            }
                            else
                            {
                                tok->buff += wcslen(tok->buff);
                                if ((!tok->accept_unfinished))
                                {
                                    TOK_CALL_ERROR(tok, TOK_UNTERMINATED_QUOTE, QUOTE_ERROR);
                                    return;
                                }
                                do_loop = 0;
                            }

                            break;
                        }

                        case L'(':
                            paran_count++;
                            break;
                        case L')':
                            paran_count--;
                            if (paran_count == 0)
                            {
                                mode = (mode == mode_array_brackets_and_subshell ? mode_array_brackets : mode_regular_text);
                            }
                            break;
                        case L'\0':
                            do_loop = 0;
                            break;
                    }
                    break;

                case mode_array_brackets:
                    switch (*tok->buff)
                    {
                        case L'(':
                            paran_count=1;
                            mode = mode_array_brackets_and_subshell;
                            break;

                        case L']':
                            mode = mode_regular_text;
                            break;

                        case L'\0':
                            do_loop = 0;
                            break;
                    }
                    break;
            }
        }


        if (!do_loop)
            break;

        tok->buff++;
        is_first = false;
    }

    if ((!tok->accept_unfinished) && (mode != mode_regular_text))
    {
        switch (mode)
        {
            case mode_subshell:
                TOK_CALL_ERROR(tok, TOK_UNTERMINATED_SUBSHELL, PARAN_ERROR);
                break;
            case mode_array_brackets:
            case mode_array_brackets_and_subshell:
                TOK_CALL_ERROR(tok, TOK_UNTERMINATED_SUBSHELL, SQUARE_BRACKET_ERROR); // TOK_UNTERMINATED_SUBSHELL is a lie but nobody actually looks at it
                break;
            default:
                assert(0 && "Unexpected mode in read_string");
                break;
        }
        return;
    }


    len = tok->buff - start;

    tok->last_token.assign(start, len);
    tok->last_type = TOK_STRING;
}

/**
   Read the next token as a comment.
*/
static void read_comment(tokenizer_t *tok)
{
    const wchar_t *start;

    start = tok->buff;
    while (*(tok->buff)!= L'\n' && *(tok->buff)!= L'\0')
        tok->buff++;


    size_t len = tok->buff - start;
    tok->last_token.assign(start, len);
    tok->last_type = TOK_COMMENT;
}



/* Reads a redirection or an "fd pipe" (like 2>|) from a string. Returns how many characters were consumed. If zero, then this string was not a redirection.

   Also returns by reference the redirection mode, and the fd to redirection. If there is overflow, *out_fd is set to -1.
*/
static size_t read_redirection_or_fd_pipe(const wchar_t *buff, enum token_type *out_redirection_mode, int *out_fd)
{
    bool errored = false;
    int fd = 0;
    enum token_type redirection_mode = TOK_NONE;

    size_t idx = 0;

    /* Determine the fd. This may be specified as a prefix like '2>...' or it may be implicit like '>' or '^'. Try parsing out a number; if we did not get any digits then infer it from the first character. Watch out for overflow. */
    long long big_fd = 0;
    for (; iswdigit(buff[idx]); idx++)
    {
        /* Note that it's important we consume all the digits here, even if it overflows. */
        if (big_fd <= INT_MAX)
            big_fd = big_fd * 10 + (buff[idx] - L'0');
    }

    fd = (big_fd > INT_MAX ? -1 : static_cast<int>(big_fd));

    if (idx == 0)
    {
        /* We did not find a leading digit, so there's no explicit fd. Infer it from the type */
        switch (buff[idx])
        {
            case L'>':
                fd = STDOUT_FILENO;
                break;
            case L'<':
                fd = STDIN_FILENO;
                break;
            case L'^':
                fd = STDERR_FILENO;
                break;
            default:
                errored = true;
                break;
        }
    }

    /* Either way we should have ended on the redirection character itself like '>' */
    wchar_t redirect_char = buff[idx++]; //note increment of idx
    if (redirect_char == L'>' || redirect_char == L'^')
    {
        redirection_mode = TOK_REDIRECT_OUT;
        if (buff[idx] == redirect_char)
        {
            /* Doubled up like ^^ or >>. That means append */
            redirection_mode = TOK_REDIRECT_APPEND;
            idx++;
        }
    }
    else if (redirect_char == L'<')
    {
        redirection_mode = TOK_REDIRECT_IN;
    }
    else
    {
        /* Something else */
        errored = true;
    }

    /* Optional characters like & or ?, or the pipe char | */
    wchar_t opt_char = buff[idx];
    if (opt_char == L'&')
    {
        redirection_mode = TOK_REDIRECT_FD;
        idx++;
    }
    else if (opt_char == L'?')
    {
        redirection_mode = TOK_REDIRECT_NOCLOB;
        idx++;
    }
    else if (opt_char == L'|')
    {
        /* So the string looked like '2>|'. This is not a redirection - it's a pipe! That gets handled elsewhere. */
        redirection_mode = TOK_PIPE;
        idx++;
    }

    /* Don't return valid-looking stuff on error */
    if (errored)
    {
        idx = 0;
        redirection_mode = TOK_NONE;
    }

    /* Return stuff */
    if (out_redirection_mode != NULL)
        *out_redirection_mode = redirection_mode;
    if (out_fd != NULL)
        *out_fd = fd;

    return idx;
}

enum token_type redirection_type_for_string(const wcstring &str, int *out_fd)
{
    enum token_type mode = TOK_NONE;
    int fd = 0;
    read_redirection_or_fd_pipe(str.c_str(), &mode, &fd);
    /* Redirections only, no pipes */
    if (mode == TOK_PIPE || fd < 0)
        mode = TOK_NONE;
    if (out_fd != NULL)
        *out_fd = fd;
    return mode;
}

int fd_redirected_by_pipe(const wcstring &str)
{
    /* Hack for the common case */
    if (str == L"|")
    {
        return STDOUT_FILENO;
    }

    enum token_type mode = TOK_NONE;
    int fd = 0;
    read_redirection_or_fd_pipe(str.c_str(), &mode, &fd);
    /* Pipes only */
    if (mode != TOK_PIPE || fd < 0)
        fd = -1;
    return fd;
}

int oflags_for_redirection_type(enum token_type type)
{
    switch (type)
    {
        case TOK_REDIRECT_APPEND:
            return O_CREAT | O_APPEND | O_WRONLY;
        case TOK_REDIRECT_OUT:
            return O_CREAT | O_WRONLY | O_TRUNC;
        case TOK_REDIRECT_NOCLOB:
            return O_CREAT | O_EXCL | O_WRONLY;
        case TOK_REDIRECT_IN:
            return O_RDONLY;

        default:
            return -1;
    }
}

/**
   Test if a character is whitespace. Differs from iswspace in that it
   does not consider a newline to be whitespace.
*/
static bool my_iswspace(wchar_t c)
{
    return c != L'\n' && iswspace(c);
}


const wchar_t *tok_get_desc(int type)
{
    if (type < 0 || (size_t)type >= (sizeof tok_desc / sizeof *tok_desc))
    {
        return _(L"Invalid token type");
    }
    return _(tok_desc[type]);
}

void tok_next(tokenizer_t *tok)
{

    CHECK(tok,);
    CHECK(tok->buff,);

    if (tok_last_type(tok) == TOK_ERROR)
    {
        tok->has_next=false;
        return;
    }

    if (!tok->has_next)
    {
        /*    wprintf( L"EOL\n" );*/
        tok->last_type = TOK_END;
        return;
    }

    while (1)
    {
        if (tok->buff[0] == L'\\' && tok->buff[1] == L'\n')
        {
            tok->buff += 2;
            tok->continue_line_after_comment = true;
        }
        else if (my_iswspace(tok->buff[0]))
        {
            tok->buff++;
        }
        else
        {
            break;
        }
    }


    while (*tok->buff == L'#')
    {
        if (tok->show_comments)
        {
            tok->last_pos = tok->buff - tok->orig_buff;
            read_comment(tok);

            if (tok->buff[0] == L'\n' && tok->continue_line_after_comment)
                tok->buff++;

            return;
        }
        else
        {
            while (*(tok->buff)!= L'\n' && *(tok->buff)!= L'\0')
                tok->buff++;

            if (tok->buff[0] == L'\n' && tok->continue_line_after_comment)
                tok->buff++;
        }

        while (my_iswspace(*(tok->buff))) {
            tok->buff++;
        }
    }

    tok->continue_line_after_comment = false;

    tok->last_pos = tok->buff - tok->orig_buff;

    switch (*tok->buff)
    {
        case L'\0':
            tok->last_type = TOK_END;
            /*fwprintf( stderr, L"End of string\n" );*/
            tok->has_next = false;
            break;
        case 13: // carriage return
        case L'\n':
        case L';':
            tok->last_type = TOK_END;
            tok->buff++;
            // Hack: when we get a newline, swallow as many as we can
            // This compresses multiple subsequent newlines into a single one
            if (! tok->show_blank_lines)
            {
                while (*tok->buff == L'\n' || *tok->buff == 13 /* CR */ || *tok->buff == ' ' || *tok->buff == '\t')
                {
                    tok->buff++;
                }
            }
            tok->last_token.clear();
            break;
        case L'&':
            tok->last_type = TOK_BACKGROUND;
            tok->buff++;
            break;

        case L'|':
            tok->last_token = L"1";
            tok->last_type = TOK_PIPE;
            tok->buff++;
            break;

        case L'>':
        case L'<':
        case L'^':
        {
            /* There's some duplication with the code in the default case below. The key difference here is that we must never parse these as a string; a failed redirection is an error! */
            enum token_type mode = TOK_NONE;
            int fd = -1;
            size_t consumed = read_redirection_or_fd_pipe(tok->buff, &mode, &fd);
            if (consumed == 0 || fd < 0)
            {
                TOK_CALL_ERROR(tok, TOK_OTHER, REDIRECT_ERROR);
            }
            else
            {
                tok->buff += consumed;
                tok->last_type = mode;
                tok->last_token = to_string(fd);
            }
        }
        break;

        default:
        {
            /* Maybe a redirection like '2>&1', maybe a pipe like 2>|, maybe just a string */
            size_t consumed = 0;
            enum token_type mode = TOK_NONE;
            int fd = -1;
            if (iswdigit(*tok->buff))
                consumed = read_redirection_or_fd_pipe(tok->buff, &mode, &fd);

            if (consumed > 0)
            {
                /* It looks like a redirection or a pipe. But we don't support piping fd 0. Note that fd 0 may be -1, indicating overflow; but we don't treat that as a tokenizer error. */
                if (mode == TOK_PIPE && fd == 0)
                {
                    TOK_CALL_ERROR(tok, TOK_OTHER, PIPE_ERROR);
                }
                else
                {
                    tok->buff += consumed;
                    tok->last_type = mode;
                    tok->last_token = to_string(fd);
                }
            }
            else
            {
                /* Not a redirection or pipe, so just a string */
                read_string(tok);
            }
        }
        break;

    }

}

wcstring tok_first(const wchar_t *str)
{
    wcstring result;
    if (str)
    {
        tokenizer_t t(str, TOK_SQUASH_ERRORS);
        switch (tok_last_type(&t))
        {
            case TOK_STRING:
            {
                const wchar_t *tmp = tok_last(&t);
                if (tmp != NULL)
                    result = tmp;
                break;
            }
            default:
                break;
        }
    }
    return result;
}

int tok_get_pos(const tokenizer_t *tok)
{
    CHECK(tok, 0);
    return (int)tok->last_pos;
}

size_t tok_get_extent(const tokenizer_t *tok)
{
    CHECK(tok, 0);
    size_t current_pos = tok->buff - tok->orig_buff;
    return current_pos > tok->last_pos ? current_pos - tok->last_pos : 0;
}


void tok_set_pos(tokenizer_t *tok, int pos)
{
    CHECK(tok,);

    tok->buff = tok->orig_buff + pos;
    tok->has_next = true;
    tok_next(tok);
}

bool move_word_state_machine_t::consume_char_punctuation(wchar_t c)
{
    enum
    {
        s_always_one = 0,
        s_whitespace,
        s_alphanumeric,
        s_end
    };

    bool consumed = false;
    while (state != s_end && ! consumed)
    {
        switch (state)
        {
            case s_always_one:
                /* Always consume the first character */
                consumed = true;
                state = s_whitespace;
                break;

            case s_whitespace:
                if (iswspace(c))
                {
                    /* Consumed whitespace */
                    consumed = true;
                }
                else
                {
                    state = s_alphanumeric;
                }
                break;

            case s_alphanumeric:
                if (iswalnum(c))
                {
                    /* Consumed alphanumeric */
                    consumed = true;
                }
                else
                {
                    state = s_end;
                }
                break;

            case s_end:
            default:
                break;
        }
    }
    return consumed;
}

bool move_word_state_machine_t::is_path_component_character(wchar_t c)
{
    /* Always treat separators as first. All this does is ensure that we treat ^ as a string character instead of as stderr redirection, which I hypothesize is usually what is desired. */
    return tok_is_string_character(c, true) && ! wcschr(L"/={,}'\"", c);
}

bool move_word_state_machine_t::consume_char_path_components(wchar_t c)
{
    enum
    {
        s_initial_punctuation,
        s_whitespace,
        s_separator,
        s_slash,
        s_path_component_characters,
        s_end
    };

    //printf("state %d, consume '%lc'\n", state, c);
    bool consumed = false;
    while (state != s_end && ! consumed)
    {
        switch (state)
        {
            case s_initial_punctuation:
                if (! is_path_component_character(c))
                {
                    consumed = true;
                }
                state = s_whitespace;
                break;

            case s_whitespace:
                if (iswspace(c))
                {
                    /* Consumed whitespace */
                    consumed = true;
                }
                else if (c == L'/' || is_path_component_character(c))
                {
                    /* Path component */
                    state = s_slash;
                }
                else
                {
                    /* Path separator */
                    state = s_separator;
                }
                break;

            case s_separator:
                if (! iswspace(c) && ! is_path_component_character(c))
                {
                    /* Consumed separator */
                    consumed = true;
                }
                else
                {
                    state = s_end;
                }
                break;

            case s_slash:
                if (c == L'/')
                {
                    /* Consumed slash */
                    consumed = true;
                }
                else
                {
                    state = s_path_component_characters;
                }
                break;

            case s_path_component_characters:
                if (is_path_component_character(c))
                {
                    /* Consumed string character except slash */
                    consumed = true;
                }
                else
                {
                    state = s_end;
                }
                break;

                /* We won't get here, but keep the compiler happy */
            case s_end:
            default:
                break;
        }
    }
    return consumed;
}

bool move_word_state_machine_t::consume_char(wchar_t c)
{
    switch (style)
    {
        case move_word_style_punctuation:
            return consume_char_punctuation(c);
        case move_word_style_path_components:
            return consume_char_path_components(c);
        default:
            return false;
    }
}

move_word_state_machine_t::move_word_state_machine_t(move_word_style_t syl) : state(0), style(syl)
{
}

void move_word_state_machine_t::reset()
{
    state = 0;
}
