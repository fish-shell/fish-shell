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
   Error string for invalid redirections
*/
#define REDIRECT_ERROR _( L"Invalid input/output redirection" )

/**
   Error string for when trying to pipe from fd 0
*/
#define PIPE_ERROR _( L"Can not use fd 0 as pipe output" )

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
    N_(L"Invalid token"),
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


tokenizer_t::tokenizer_t(const wchar_t *b, tok_flags_t flags) : buff(NULL), orig_buff(NULL), last_type(0), last_pos(0), has_next(false), accept_unfinished(false), show_comments(false), last_quote(0), error(0), squash_errors(false), cached_lineno_offset(0), cached_lineno_count(0)
{

    /* We can only generate error messages on the main thread due to wgettext() thread safety issues. */
    if (!(flags & TOK_SQUASH_ERRORS))
    {
        ASSERT_IS_MAIN_THREAD();
    }

    CHECK(b,);


    this->accept_unfinished = !!(flags & TOK_ACCEPT_UNFINISHED);
    this->show_comments = !!(flags & TOK_SHOW_COMMENTS);
    this->squash_errors = !!(flags & TOK_SQUASH_ERRORS);

    this->has_next = (*b != L'\0');
    this->orig_buff = this->buff = b;
    this->cached_lineno_offset = 0;
    this->cached_lineno_count = 0;
    tok_next(this);
}

int tok_last_type(tokenizer_t *tok)
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

int tokenizer_t::line_number_of_character_at_offset(size_t offset)
{
    // we want to return (one plus) the number of newlines at offsets less than the given offset
    // cached_lineno_count is the number of newlines at indexes less than cached_lineno_offset
    const wchar_t *str = orig_buff;
    if (! str)
        return 0;

    // easy hack to handle 0
    if (offset == 0)
        return 1;

    size_t i;
    if (offset > cached_lineno_offset)
    {
        for (i = cached_lineno_offset; str[i] && i<offset; i++)
        {
            /* Add one for every newline we find in the range [cached_lineno_offset, offset) */
            if (str[i] == L'\n')
                cached_lineno_count++;
        }
        cached_lineno_offset = i; //note: i, not offset, in case offset is beyond the length of the string
    }
    else if (offset < cached_lineno_offset)
    {
        /* Subtract one for every newline we find in the range [offset, cached_lineno_offset) */
        for (i = offset; i < cached_lineno_offset; i++)
        {
            if (str[i] == L'\n')
                cached_lineno_count--;
        }
        cached_lineno_offset = offset;
    }
    return cached_lineno_count + 1;
}

/**
   Tests if this character can be a part of a string. The redirect ^ is allowed unless it's the first character.
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
        case L'#':
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
        TOK_CALL_ERROR(tok, TOK_UNTERMINATED_SUBSHELL, PARAN_ERROR);
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

/**
   Read a FD redirection.
*/
static void read_redirect(tokenizer_t *tok, int fd)
{
    int mode = -1;

    if ((*tok->buff == L'>') ||
            (*tok->buff == L'^'))
    {
        tok->buff++;
        if (*tok->buff == *(tok->buff-1))
        {
            tok->buff++;
            mode = 1;
        }
        else
        {
            mode = 0;
        }

        if (*tok->buff == L'|')
        {
            if (fd == 0)
            {
                TOK_CALL_ERROR(tok, TOK_OTHER, PIPE_ERROR);
                return;
            }
            tok->buff++;
            tok->last_token = to_string<int>(fd);
            tok->last_type = TOK_PIPE;
            return;
        }
    }
    else if (*tok->buff == L'<')
    {
        tok->buff++;
        mode = 2;
    }
    else
    {
        TOK_CALL_ERROR(tok, TOK_OTHER, REDIRECT_ERROR);
    }

    tok->last_token = to_string(fd);

    if (*tok->buff == L'&')
    {
        tok->buff++;
        tok->last_type = TOK_REDIRECT_FD;
    }
    else if (*tok->buff == L'?')
    {
        tok->buff++;
        tok->last_type = TOK_REDIRECT_NOCLOB;
    }
    else
    {
        tok->last_type = TOK_REDIRECT_OUT + mode;
    }
}

wchar_t tok_last_quote(tokenizer_t *tok)
{
    CHECK(tok, 0);

    return tok->last_quote;
}

/**
   Test if a character is whitespace. Differs from iswspace in that it
   does not consider a newline to be whitespace.
*/
static int my_iswspace(wchar_t c)
{
    if (c == L'\n')
        return 0;
    else
        return iswspace(c);
}


const wchar_t *tok_get_desc(int type)
{
    if (type < 0 || (size_t)type >= sizeof(tok_desc))
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
        if (my_iswspace(*(tok->buff)))
        {
            tok->buff++;
        }
        else
        {
            if ((*(tok->buff) == L'\\') &&(*(tok->buff+1) == L'\n'))
            {
                tok->last_pos = tok->buff - tok->orig_buff;
                tok->buff+=2;
                tok->last_type = TOK_END;
                return;
            }
            break;
        }
    }


    if (*tok->buff == L'#')
    {
        if (tok->show_comments)
        {
            tok->last_pos = tok->buff - tok->orig_buff;
            read_comment(tok);
            return;
        }
        else
        {
            while (*(tok->buff)!= L'\n' && *(tok->buff)!= L'\0')
                tok->buff++;
        }

        while (my_iswspace(*(tok->buff)))
            tok->buff++;
    }

    tok->last_pos = tok->buff - tok->orig_buff;

    switch (*tok->buff)
    {

        case L'\0':
            tok->last_type = TOK_END;
            /*fwprintf( stderr, L"End of string\n" );*/
            tok->has_next = false;
            break;
        case 13:
        case L'\n':
        case L';':
            tok->last_type = TOK_END;
            tok->buff++;
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
            read_redirect(tok, 1);
            return;
        case L'<':
            read_redirect(tok, 0);
            return;
        case L'^':
            read_redirect(tok, 2);
            return;

        default:
        {
            if (iswdigit(*tok->buff))
            {
                const wchar_t *orig = tok->buff;
                int fd = 0;
                while (iswdigit(*tok->buff))
                    fd = (fd*10) + (*(tok->buff++) - L'0');

                switch (*(tok->buff))
                {
                    case L'^':
                    case L'>':
                    case L'<':
                        read_redirect(tok, fd);
                        return;
                }
                tok->buff = orig;
            }
            read_string(tok);
        }

    }

}

const wchar_t *tok_string(tokenizer_t *tok)
{
    return tok?tok->orig_buff:0;
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

int tok_get_pos(tokenizer_t *tok)
{
    CHECK(tok, 0);

    return (int)tok->last_pos;
}


void tok_set_pos(tokenizer_t *tok, int pos)
{
    CHECK(tok,);

    tok->buff = tok->orig_buff + pos;
    tok->has_next = true;
    tok_next(tok);
}


#ifdef TOKENIZER_TEST

/**
   This main function is used for compiling the tokenizer_test command, used for testing the tokenizer.
*/
int main(int argc, char **argv)
{
    tokenizer tok;
    int i;
    for (i=1; i<argc; i++)
    {
        wprintf(L"Tokenizing string %s\n", argv[i]);
        for (tok_init(&tok, str2wcs(argv[i]), 0); tok_has_next(&tok); tok_next(&tok))
        {
            switch (tok_last_type(&tok))
            {
                case TOK_INVALID:
                    wprintf(L"Type: INVALID\n");
                    break;
                case TOK_STRING:
                    wprintf(L"Type: STRING\t Value: %ls\n", tok_last(&tok));
                    break;
                case TOK_PIPE:
                    wprintf(L"Type: PIPE\n");
                    break;
                case TOK_END:
                    wprintf(L"Type: END\n");
                    break;
                case TOK_ERROR:
                    wprintf(L"Type: ERROR\n");
                    break;
                default:
                    wprintf(L"Type: Unknown\n");
                    break;
            }
        }
    }
}

#endif
