/* printf - format and print data
   Copyright (C) 1990-2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Usage: printf format [argument...]

   A front end to the printf function that lets it be used from the shell.

   Backslash escapes:

   \" = double quote
   \\ = backslash
   \a = alert (bell)
   \b = backspace
   \c = produce no further output
   \f = form feed
   \n = new line
   \r = carriage return
   \t = horizontal tab
   \v = vertical tab
   \ooo = octal number (ooo is 1 to 3 digits)
   \xhh = hexadecimal number (hhh is 1 to 2 digits)
   \uhhhh = 16-bit Unicode character (hhhh is 4 digits)
   \Uhhhhhhhh = 32-bit Unicode character (hhhhhhhh is 8 digits)

   Additional directive:

   %b = print an argument string, interpreting backslash escapes,
     except that octal escapes are of the form \0 or \0ooo.

   The `format' argument is re-used as many times as necessary
   to convert all of the given arguments.

   David MacKenzie <djm@gnu.ai.mit.edu> */

/* This file has been imported from source code of printf command in GNU Coreutils version 6.9 */

#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>
#include <inttypes.h>

#include "common.h"

bool isodigit(const wchar_t &c)
{
    return wcschr(L"01234567", c) != NULL;
}

int hextobin(const wchar_t &c)
{
    switch (c)
    {
        case L'0':
            return 0;
        case L'1':
            return 1;
        case L'2':
            return 2;
        case L'3':
            return 3;
        case L'4':
            return 4;
        case L'5':
            return 5;
        case L'6':
            return 6;
        case L'7':
            return 7;
        case L'8':
            return 8;
        case L'9':
            return 9;
        case L'a':
            return 10;
        case L'b':
            return 11;
        case L'c':
            return 12;
        case 'd':
            return 13;
        case 'e':
            return 14;
        case 'f':
            return 15;
        default:
            append_format(stderr_buffer, L"Invalid hex number : %lc", c);
            return -1;
    }    
}

int octtobin(const wchar_t &c)
{
    switch (c)
    {
        case L'0':
            return 0;
        case L'1':
            return 1;
        case L'2':
            return 2;
        case L'3':
            return 3;
        case L'4':
            return 4;
        case L'5':
            return 5;
        case L'6':
            return 6;
        case L'7':
            return 7;
        default:
            append_format(stderr_buffer, L"Invalid octal number : %lc", c);
            return -1;
    }
}

static int exit_code;

/* True if the POSIXLY_CORRECT environment variable is set.  */
static bool posixly_correct;

/* This message appears in N_() here rather than just in _() below because
   the sole use would have been in a #define.  */
static wchar_t const *const cfcc_msg =
 N_(L"warning: %ls: character(s) following character constant have been ignored");

double C_STRTOD (wchar_t const *nptr, wchar_t **endptr)
{
    double r;

    const wcstring saved_locale = wsetlocale (LC_NUMERIC, NULL);

    if (!saved_locale.empty())
    {
        wsetlocale (LC_NUMERIC, L"C");
    }

    r = wcstod(nptr, endptr);

    if (!saved_locale.empty())
    {
        wsetlocale (LC_NUMERIC, saved_locale.c_str());
    }

    return r;
}

static inline unsigned wchar_t to_uchar (wchar_t ch)
{
    return ch;
}

static void verify_numeric (const wchar_t *s, const wchar_t *end)
{
    if (errno)
    {
        append_format(stderr_buffer, L"%ls", s);
        exit_code = EXIT_FAILURE;
    }
    else if (*end)
    {
        if (s == end)
            append_format(stderr_buffer, _(L"%ls: expected a numeric value"), s);
        else
            append_format(stderr_buffer, _(L"%ls: value not completely converted"), s);
        exit_code = EXIT_FAILURE;
    }
}

#define STRTOX(TYPE, FUNC_NAME, LIB_FUNC_EXPR)				 \
static TYPE								 \
FUNC_NAME (wchar_t const *s)						 \
{									 \
    wchar_t *end;								 \
    TYPE val;								 \
									 \
    if (*s == L'\"' || *s == L'\'')						 \
    {									 \
        unsigned wchar_t ch = *++s;						 \
        val = ch;								 \
        /* If POSIXLY_CORRECT is not set, then give a warning that there	 \
        are characters following the character constant and that GNU	 \
        printf is ignoring those characters.  If POSIXLY_CORRECT *is*	 \
        set, then don't give the warning.  */				 \
        if (*++s != 0 && !posixly_correct)				 \
            append_format(stderr_buffer, _(cfcc_msg), s);					 \
        }									 \
    else									 \
    {									 \
        errno = 0;							 \
        val = (LIB_FUNC_EXPR);						 \
        verify_numeric (s, end);						 \
    }									 \
    return val;								 \
}									 \

STRTOX (intmax_t,    vwcstoimax, wcstoimax (s, &end, 0))
STRTOX (uintmax_t,   vwcstoumax, wcstoumax (s, &end, 0))
STRTOX (long double, vwcstold, C_STRTOD(s, &end))

/* Output a single-character \ escape.  */

static void print_esc_char (wchar_t c)
{
    switch (c)
    {
    case L'a':			/* Alert. */
        append_format(stdout_buffer, L"%lc", L'\a');
        break;
        case L'b':			/* Backspace. */
        append_format(stdout_buffer, L"%lc", L'\b');
        break;
    case L'c':			/* Cancel the rest of the output. */
        return;
        break;
    case L'f':			/* Form feed. */
        append_format(stdout_buffer, L"%lc", L'\f');
        break;
    case L'n':			/* New line. */
        append_format(stdout_buffer, L"%lc", L'\n');
        break;
    case L'r':			/* Carriage retturn. */
        append_format(stdout_buffer, L"%lc", L'\r');
        break;
    case L't':			/* Horizontal tab. */
        append_format(stdout_buffer, L"%lc", L'\t');
        break;
    case L'v':			/* Vertical tab. */
        append_format(stdout_buffer, L"%lc", L'\v');
        break;
    default:
        append_format(stdout_buffer, L"%lc", c);
        break;
    }
}

/* Print a \ escape sequence starting at ESCSTART.
   Return the number of characters in the escape sequence
   besides the backslash.
   If OCTAL_0 is nonzero, octal escapes are of the form \0ooo, where o
   is an octal digit; otherwise they are of the form \ooo.  */
static int print_esc (const wchar_t *escstart, bool octal_0)
{
    const wchar_t *p = escstart + 1;
    int esc_value = 0;		/* Value of \nnn escape. */
    int esc_length;		/* Length of \nnn escape. */

    if (*p == L'x')
    {
        /* A hexadecimal \xhh escape sequence must have 1 or 2 hex. digits.  */
        for (esc_length = 0, ++p;
        esc_length < 2 && isxdigit (to_uchar (*p));
        ++esc_length, ++p)
         esc_value = esc_value * 16 + hextobin (*p);
        if (esc_length == 0)
            append_format(stderr_buffer, _(L"missing hexadecimal number in escape"));
        append_format (stdout_buffer, L"%lc", esc_value);
    }
    else if (isodigit (*p))
    {
        /* Parse \0ooo (if octal_0 && *p == L'0') or \ooo (otherwise).
        Allow \ooo if octal_0 && *p != L'0'; this is an undocumented
        extension to POSIX that is compatible with Bash 2.05b.  */
        for (esc_length = 0, p += octal_0 && *p == L'0';
            esc_length < 3 && isodigit (*p);
            ++esc_length, ++p)
            esc_value = esc_value * 8 + octtobin (*p);
        append_format(stdout_buffer, L"%c", esc_value);
     }
    else if (*p && wcschr (L"\"\\abcfnrtv", *p))
        print_esc_char (*p++);
    else if (*p == L'u' || *p == L'U')
    {
        wchar_t esc_char = *p;
        unsigned int uni_value;

        uni_value = 0;
        for (esc_length = (esc_char == L'u' ? 4 : 8), ++p;
        esc_length > 0;
        --esc_length, ++p)
        {
            if (! isxdigit (to_uchar (*p)))
                append_format(stderr_buffer, _(L"missing hexadecimal number in escape"));
            uni_value = uni_value * 16 + hextobin (*p);
        }

        /* A universal character name shall not specify a character short
        identifier in the range 00000000 through 00000020, 0000007F through
        0000009F, or 0000D800 through 0000DFFF inclusive. A universal
        character name shall not designate a character in the required
        character set.  */
        if ((uni_value <= 0x9f
        && uni_value != 0x24 && uni_value != 0x40 && uni_value != 0x60)
        || (uni_value >= 0xd800 && uni_value <= 0xdfff))
            append_format(stderr_buffer, _(L"invalid universal character name \\%c%0*x"),
            esc_char, (esc_char == L'u' ? 4 : 8), uni_value);
            append_format(stdout_buffer, L"%lc", uni_value);
    }
    else
    {
        append_format(stdout_buffer, L"%lc", L'\\');
        if (*p)
        {
            append_format (stdout_buffer, L"%lc", *p);
            p++;
        }
    }
    return p - escstart - 1;
}

/* Print string STR, evaluating \ escapes. */

static void
print_esc_string (const wchar_t *str)
{
    for (; *str; str++)
        if (*str == L'\\')
            str += print_esc (str, true);
        else
            append_format (stdout_buffer, L"%lc", *str);
}

/* Evaluate a printf conversion specification.  START is the start of
   the directive, LENGTH is its length, and CONVERSION specifies the
   type of conversion.  LENGTH does not include any length modifier or
   the conversion specifier itself.  FIELD_WIDTH and PRECISION are the
   field width and precision for '*' values, if HAVE_FIELD_WIDTH and
   HAVE_PRECISION are true, respectively.  ARGUMENT is the argument to
   be formatted.  */

static void print_direc (const wchar_t *start, size_t length, wchar_t conversion,
	     bool have_field_width, int field_width,
	     bool have_precision, int precision,
	     wchar_t const *argument)
{
    wcstring fmt;

    /*  Create a null-terminated copy of the % directive, with an
        intmax_t-wide length modifier substituted for any existing
        integer length modifier.  */
    {
        wchar_t *q;
        wchar_t const *length_modifier;
        size_t length_modifier_len;

        switch (conversion)
        {
            case L'd': case L'i':
                length_modifier = L"lld";
                length_modifier_len = sizeof L"lld" - 2;
                break;
            case L'a': case L'e': case L'f': case L'g':
            case L'A': case L'E': case L'F': case L'G':
                length_modifier = L"L";
                length_modifier_len = 1;
                break;
            case L's': case L'u':
                length_modifier = L"l";
                length_modifier_len = 1;
                break;
            default:
                length_modifier = start;  /* Any valid pointer will do.  */
                length_modifier_len = 0;
                break;
        }

        wchar_t p[length + length_modifier_len + 2];		/* Null-terminated copy of % directive. */
        q = wmemcpy(p, start,  length) + length;
        q = wmemcpy(q, length_modifier, length_modifier_len) + length_modifier_len;
        *q++ = conversion;
        *q = L'\0';
        fmt = p;
    }

    switch (conversion)
    {
        case L'd':
        case L'i':
        {
            intmax_t arg = vwcstoimax (argument);
            if (!have_field_width)
            {
                if (!have_precision)
                    append_format(stdout_buffer, fmt.c_str(), arg);
                else
                    append_format(stdout_buffer, fmt.c_str(), precision, arg);
            }
            else
            {
                if (!have_precision)
                    append_format(stdout_buffer, fmt.c_str(), field_width, arg);
                else
                    append_format(stdout_buffer, fmt.c_str(), field_width, precision, arg);
            }
        }
        break;

        case L'o':
        case L'u':
        case L'x':
        case L'X':
        {
            uintmax_t arg = vwcstoumax (argument);
            if (!have_field_width)
            {
                if (!have_precision)
                    append_format(stdout_buffer, fmt.c_str(), arg);
                else
                    append_format(stdout_buffer, fmt.c_str(), precision, arg);
            }
            else
            {
                if (!have_precision)
                    append_format(stdout_buffer, fmt.c_str(), field_width, arg);
                else
                    append_format(stdout_buffer, fmt.c_str(), field_width, precision, arg);
            }
        }
        break;

        case L'a':
        case L'A':
        case L'e':
        case L'E':
        case L'f':
        case L'F':
        case L'g':
        case L'G':
        {
            long double arg = vwcstold (argument);
            if (!have_field_width)
            {
                if (!have_precision)
                    append_format(stdout_buffer, fmt.c_str(), arg);
                else
                    append_format(stdout_buffer, fmt.c_str(), precision, arg);
            }
            else
            {
                if (!have_precision)
                    append_format(stdout_buffer, fmt.c_str(), field_width, arg);
                else
                    append_format(stdout_buffer, fmt.c_str(), field_width, precision, arg);
            }
        }
        break;

        case L'c':
            if (!have_field_width)
            append_format(stdout_buffer, fmt.c_str(), *argument);
            else
            append_format(stdout_buffer, fmt.c_str(), field_width, *argument);
            break;
        case L's':
            if (!have_field_width)
            {
                if (!have_precision){
                    append_format(stdout_buffer, fmt.c_str(), argument);}
                else
                    append_format(stdout_buffer, fmt.c_str(), precision, argument);
            }
            else
            {
                if (!have_precision)
                    append_format(stdout_buffer, fmt.c_str(), field_width, argument);
                else
                    append_format(stdout_buffer, fmt.c_str(), field_width, precision, argument);
            }
            break;
    }
}

/* Print the text in FORMAT, using ARGV (with ARGC elements) for
   arguments to any `%' directives.
   Return the number of elements of ARGV used.  */

static int print_formatted (const wchar_t *format, int argc, wchar_t **argv) {
    int save_argc = argc;		/* Preserve original value.  */
    const wchar_t *f;		/* Pointer into `format'.  */
    const wchar_t *direc_start;	/* Start of % directive.  */
    size_t direc_length;		/* Length of % directive.  */
    bool have_field_width;	/* True if FIELD_WIDTH is valid.  */
    int field_width = 0;		/* Arg to first '*'.  */
    bool have_precision;		/* True if PRECISION is valid.  */
    int precision = 0;		/* Arg to second '*'.  */
    bool ok[UCHAR_MAX + 1] = { };	/* ok['x'] is true if %x is allowed.  */

    for (f = format; *f != L'\0'; ++f)
    {
        switch (*f)
        {
            case L'%':
                direc_start = f++;
                direc_length = 1;
                have_field_width = have_precision = false;
                if (*f == L'%')
                {
                    append_format(stdout_buffer, L"%lc", L'%');
                    break;
                }
                if (*f == L'b')
                {
                    /* FIXME: Field width and precision are not supported
                    for %b, even though POSIX requires it.  */
                    if (argc > 0)
                    {
                        print_esc_string (*argv);
                        ++argv;
                        --argc;
                    }
                    break;
                }

                ok['a'] = ok['A'] = ok['c'] = ok['d'] = ok['e'] = ok['E'] =
                ok['f'] = ok['F'] = ok['g'] = ok['G'] = ok['i'] = ok['o'] =
                ok['s'] = ok['u'] = ok['x'] = ok['X'] = true;

                for (;; f++, direc_length++)
                {
                    switch (*f)
                    {
#if (__GLIBC__ == 2 && 2 <= __GLIBC_MINOR__) || 3 <= __GLIBC__
                        case L'I':
#endif
                        case L'\'':
                        ok['a'] = ok['A'] = ok['c'] = ok['e'] = ok['E'] =
                        ok['o'] = ok['s'] = ok['x'] = ok['X'] = false;
                        break;
                        case '-': case '+': case ' ':
                        break;
                        case L'#':
                        ok['c'] = ok['d'] = ok['i'] = ok['s'] = ok['u'] = false;
                        break;
                        case '0':
                        ok['c'] = ok['s'] = false;
                        break;
                        default:
                        goto no_more_flag_characters;
                    }
                }
                no_more_flag_characters:;

                if (*f == L'*')
                {
                    ++f;
                    ++direc_length;
                    if (argc > 0)
                    {
                        intmax_t width = vwcstoimax (*argv);
                        if (INT_MIN <= width && width <= INT_MAX)
                            field_width = width;
                        else
                            append_format(stderr_buffer, _(L"invalid field width: %ls"), *argv);
                        ++argv;
                        --argc;
                    }
                    else
                    {
                        field_width = 0;
                    }
                    have_field_width = true;
                }
                else
                {
                    while (iswdigit(*f))
                    {
                        ++f;
                        ++direc_length;
                    }
                }
                if (*f == L'.')
                {
                    ++f;
                    ++direc_length;
                    ok['c'] = false;
                    if (*f == L'*')
                    {
                        ++f;
                        ++direc_length;
                        if (argc > 0)
                        {
                            intmax_t prec = vwcstoimax (*argv);
                            if (prec < 0)
                            {
                            /* A negative precision is taken as if the
                            precision were omitted, so -1 is safe
                            here even if prec < INT_MIN.  */
                            precision = -1;
                            }
                            else if (INT_MAX < prec)
                                append_format(stderr_buffer, _(L"invalid precision: %ls"), *argv);
                            else
                            {
                                precision = prec;
                            }
                            ++argv;
                            --argc;
                        }
                    else
                    {
                        precision = 0;
                    }
                    have_precision = true;
                }
                else
                {
                    while (iswdigit(*f))
                    {
                        ++f;
                        ++direc_length;
                    }
                }
            }

        while (*f == L'l' || *f == L'L' || *f == L'h'
            || *f == L'j' || *f == L't' || *f == L'z')
            ++f;
        {
            unsigned wchar_t conversion = *f;
            if (! ok[conversion])
            append_format(stderr_buffer,
                _("%.*ls: invalid conversion specification"),
                (int) (f + 1 - direc_start), direc_start);
        }

        print_direc (direc_start, direc_length, *f,
            have_field_width, field_width,
            have_precision, precision,
            (argc <= 0 ? L"" : (argc--, *argv++)));
            break;

            case L'\\':
                f += print_esc (f, false);
                break;

            default:
                append_format (stdout_buffer, L"%lc", *f);
            }
        }
    return save_argc - argc;
}

static int builtin_printf(parser_t &parser, wchar_t **argv)
{
    wchar_t *format;
    int args_used;
    int argc = builtin_count_args(argv);

    exit_code = EXIT_SUCCESS;

    if (argc <= 1)
    {
        append_format(stderr_buffer, _(L"missing operand"));
        return EXIT_FAILURE;
    }

    format = argv[1];
    argc -= 2;
    argv += 2;

    do
    {
        args_used = print_formatted (format, argc, argv);
        argc -= args_used;
        argv += args_used;
    }
    while (args_used > 0 && argc > 0);
	return exit_code;
}
