/**
   This file only contains fallback implementations of functions which
   have been found to be missing or broken by the configuration
   scripts.

   Many of these functions are more or less broken and
   incomplete. lrand28_r internally uses the regular (bad) rand_r
   function, the gettext function doesn't actually do anything, etc.
*/
#include "config.h"

// IWYU likes to recommend adding term.h when we want ncurses.h.
// IWYU pragma: no_include term.h
#include <stdlib.h>
#include <stdio.h>  // IWYU pragma: keep
#include <unistd.h>
#include <sys/types.h>  // IWYU pragma: keep
#include <sys/stat.h>  // IWYU pragma: keep
#include <errno.h>  // IWYU pragma: keep
#include <fcntl.h>  // IWYU pragma: keep
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <dirent.h>  // IWYU pragma: keep
#include <stdarg.h>  // IWYU pragma: keep
#include <limits.h>  // IWYU pragma: keep
#include <assert.h>  // IWYU pragma: keep
#if HAVE_GETTEXT
#include <libintl.h>
#endif
#if HAVE_NCURSES_H
#include <ncurses.h>  // IWYU pragma: keep
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>  // IWYU pragma: keep
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <signal.h>  // IWYU pragma: keep
#include <wchar.h>  // IWYU pragma: keep

#include "fallback.h"  // IWYU pragma: keep
#include "util.h"  // IWYU pragma: keep

#ifndef HAVE___ENVIRON

char **__environ = 0;

#endif

#ifdef TPUTS_KLUDGE

int tputs(const char *str, int affcnt, int (*fish_putc)(tputs_arg_t))
{
    while (*str)
    {
        fish_putc(*str++);
    }
}

#endif

#ifdef TPARM_SOLARIS_KLUDGE

#undef tparm

/**
   Checks for known string values and maps to correct number of parameters.
*/
char *tparm_solaris_kludge(char *str, ...)
{
    long int param[9] = { };

    va_list ap;
    va_start(ap, str);

    if ((set_a_foreground && ! strcmp(str, set_a_foreground))
            || (set_a_background && ! strcmp(str, set_a_background))
            || (set_foreground && ! strcmp(str, set_foreground))
            || (set_background && ! strcmp(str, set_background))
            || (enter_underline_mode && ! strcmp(str, enter_underline_mode))
            || (exit_underline_mode && ! strcmp(str, exit_underline_mode))
            || (enter_standout_mode && ! strcmp(str, enter_standout_mode))
            || (exit_standout_mode && ! strcmp(str, exit_standout_mode))
            || (flash_screen && ! strcmp(str, flash_screen))
            || (enter_subscript_mode && ! strcmp(str, enter_subscript_mode))
            || (exit_subscript_mode && ! strcmp(str, exit_subscript_mode))
            || (enter_superscript_mode && ! strcmp(str, enter_superscript_mode))
            || (exit_superscript_mode && ! strcmp(str, exit_superscript_mode))
            || (enter_blink_mode && ! strcmp(str, enter_blink_mode))
            || (enter_italics_mode && ! strcmp(str, enter_italics_mode))
            || (exit_italics_mode && ! strcmp(str, exit_italics_mode))
            || (enter_reverse_mode && ! strcmp(str, enter_reverse_mode))
            || (enter_shadow_mode && ! strcmp(str, enter_shadow_mode))
            || (exit_shadow_mode && ! strcmp(str, exit_shadow_mode))
            || (enter_secure_mode && ! strcmp(str, enter_secure_mode))
            || (enter_bold_mode && ! strcmp(str, enter_bold_mode)))
    {
        param[0] = va_arg(ap, long int);
    }
    else if (cursor_address && ! strcmp(str, cursor_address))
    {
        param[0] = va_arg(ap, long int);
        param[1] = va_arg(ap, long int);
    }

    va_end(ap);


    return tparm(str, param[0], param[1], param[2], param[3],
                 param[4], param[5],  param[6], param[7], param[8]);
}

// Re-defining just to make sure nothing breaks further down in this file.
#define tparm tparm_solaris_kludge

#endif


#ifndef HAVE_FWPRINTF
#define INTERNAL_FWPRINTF 1
#endif

#ifdef HAVE_BROKEN_FWPRINTF
#define INTERNAL_FWPRINTF 1
#endif

#ifdef INTERNAL_FWPRINTF

/**
   Internal function for the wprintf fallbacks. USed to write the
   specified number of spaces.
*/
static void pad(void (*writer)(wchar_t), int count)
{

    int i;
    if (count < 0)
        return;

    for (i=0; i<count; i++)
    {
        writer(L' ');
    }
}

/**
   Generic formatting function. All other string formatting functions
   are secretly a wrapper around this function. vgprintf does not
   implement all the filters supported by printf, only those that are
   currently used by fish. vgprintf internally uses snprintf to
   implement the number outputs, such as %f and %x.

   Currently supported functionality:

   - precision specification, both through .* and .N
   - Padding through * and N
   - Right padding using the - prefix
   - long versions of all filters thorugh l and ll prefix
   - Character output using %c
   - String output through %s
   - Floating point number output through %f
   - Integer output through %d, %i, %u, %o, %x and %X

   For a full description on the usage of *printf, see use 'man 3 printf'.
*/
static int vgwprintf(void (*writer)(wchar_t),
                     const wchar_t *filter,
                     va_list va)
{
    const wchar_t *filter_org=filter;
    int count=0;

    for (; *filter; filter++)
    {
        if (*filter == L'%')
        {
            int is_long=0;
            int width = -1;
            filter++;
            int loop=1;
            int precision=-1;
            int pad_left = 1;

            if (iswdigit(*filter))
            {
                width=0;
                while ((*filter >= L'0') && (*filter <= L'9'))
                {
                    width=10*width+(*filter++ - L'0');
                }
            }

            while (loop)
            {

                switch (*filter)
                {
                    case L'l':
                        /* Long variable */
                        is_long++;
                        filter++;
                        break;

                    case L'*':
                        /* Set minimum field width */
                        width = va_arg(va, int);
                        filter++;
                        break;

                    case L'-':
                        filter++;
                        pad_left=0;
                        break;

                    case L'.':
                        /*
                           Set precision.
                        */
                        filter++;
                        if (*filter == L'*')
                        {
                            precision = va_arg(va, int);
                        }
                        else
                        {
                            precision=0;
                            while ((*filter >= L'0') && (*filter <= L'9'))
                            {
                                precision=10*precision+(*filter++ - L'0');
                            }
                        }
                        break;

                    default:
                        loop=0;
                        break;
                }
            }

            switch (*filter)
            {
                case L'c':
                {
                    wchar_t c;

                    if ((width >= 0) && pad_left)
                    {
                        pad(writer, width-1);
                        count += maxi(width-1, 0);
                    }

                    c = is_long?va_arg(va, wint_t):btowc(va_arg(va, int));
                    if (precision != 0)
                        writer(c);


                    if ((width >= 0) && !pad_left)
                    {
                        pad(writer, width-1);
                        count += maxi(width-1, 0);
                    }

                    count++;

                    break;
                }
                case L's':
                {

                    wchar_t *ss=0;
                    wcstring wide_ss;
                    if (is_long)
                    {
                        ss = va_arg(va, wchar_t *);
                    }
                    else
                    {
                        char *ns = va_arg(va, char*);

                        if (ns)
                        {
                            wide_ss = str2wcstring(ns);
                            ss = wide_ss.c_str();
                        }
                    }

                    if (!ss)
                    {
                        return -1;
                    }

                    if ((width >= 0) && pad_left)
                    {
                        pad(writer, width-wcslen(ss));
                        count += maxi(width-wcslen(ss), 0);
                    }

                    wchar_t *s=ss;
                    int precount = count;

                    while (*s)
                    {
                        if ((precision > 0) && (precision <= (count-precount)))
                            break;

                        writer(*(s++));
                        count++;
                    }

                    if ((width >= 0) && !pad_left)
                    {
                        pad(writer, width-wcslen(ss));
                        count += maxi(width-wcslen(ss), 0);
                    }

                    break;
                }

                case L'd':
                case L'i':
                case L'o':
                case L'u':
                case L'x':
                case L'X':
                {
                    char str[33];
                    char *pos;
                    char format[16];
                    int len;

                    format[0]=0;
                    strcat(format, "%");
                    if (precision >= 0)
                        strcat(format, ".*");
                    switch (is_long)
                    {
                        case 2:
                            strcat(format, "ll");
                            break;
                        case 1:
                            strcat(format, "l");
                            break;
                    }

                    len = strlen(format);
                    format[len++]=(char)*filter;
                    format[len]=0;

                    switch (*filter)
                    {
                        case L'd':
                        case L'i':
                        {

                            switch (is_long)
                            {
                                case 0:
                                {
                                    int d = va_arg(va, int);
                                    if (precision >= 0)
                                        snprintf(str, 32, format, precision, d);
                                    else
                                        snprintf(str, 32, format, d);

                                    break;
                                }

                                case 1:
                                {
                                    long d = va_arg(va, long);
                                    if (precision >= 0)
                                        snprintf(str, 32, format, precision, d);
                                    else
                                        snprintf(str, 32, format, d);
                                    break;
                                }

                                case 2:
                                {
                                    long long d = va_arg(va, long long);
                                    if (precision >= 0)
                                        snprintf(str, 32, format, precision, d);
                                    else
                                        snprintf(str, 32, format, d);
                                    break;
                                }

                                default:
                                    debug(0, L"Invalid length modifier in string %ls\n", filter_org);
                                    return -1;
                            }
                            break;

                        }

                        case L'u':
                        case L'o':
                        case L'x':
                        case L'X':
                        {

                            switch (is_long)
                            {
                                case 0:
                                {
                                    unsigned d = va_arg(va, unsigned);
                                    if (precision >= 0)
                                        snprintf(str, 32, format, precision, d);
                                    else
                                        snprintf(str, 32, format, d);
                                    break;
                                }

                                case 1:
                                {
                                    unsigned long d = va_arg(va, unsigned long);
                                    if (precision >= 0)
                                        snprintf(str, 32, format, precision, d);
                                    else
                                        snprintf(str, 32, format, d);
                                    break;
                                }

                                case 2:
                                {
                                    unsigned long long d = va_arg(va, unsigned long long);
                                    if (precision >= 0)
                                        snprintf(str, 32, format, precision, d);
                                    else
                                        snprintf(str, 32, format, d);
                                    break;
                                }

                                default:
                                    debug(0, L"Invalid length modifier in string %ls\n", filter_org);
                                    return -1;
                            }
                            break;

                        }

                        default:
                            debug(0, L"Invalid filter %ls in string %ls\n", *filter, filter_org);
                            return -1;

                    }

                    if ((width >= 0) && pad_left)
                    {
                        int l = maxi(width-strlen(str), 0);
                        pad(writer, l);
                        count += l;
                    }

                    pos = str;

                    while (*pos)
                    {
                        writer(*(pos++));
                        count++;
                    }

                    if ((width >= 0) && !pad_left)
                    {
                        int l = maxi(width-strlen(str), 0);
                        pad(writer, l);
                        count += l;
                    }

                    break;
                }

                case L'f':
                {
                    char str[32];
                    char *pos;
                    double val = va_arg(va, double);

                    if (precision>= 0)
                    {
                        if (width>= 0)
                        {
                            snprintf(str, 32, "%*.*f", width, precision, val);
                        }
                        else
                        {
                            snprintf(str, 32, "%.*f", precision, val);
                        }
                    }
                    else
                    {
                        if (width>= 0)
                        {
                            snprintf(str, 32, "%*f", width, val);
                        }
                        else
                        {
                            snprintf(str, 32, "%f", val);
                        }
                    }

                    pos = str;

                    while (*pos)
                    {
                        writer(*(pos++));
                        count++;
                    }

                    break;
                }

                case L'n':
                {
                    int *n = va_arg(va, int *);

                    *n = count;
                    break;
                }
                case L'%':
                {
                    writer('%');
                    count++;
                    break;
                }
                default:
                    debug(0, L"Unknown switch %lc in string %ls\n", *filter, filter_org);
                    return -1;
            }
        }
        else
        {
            writer(*filter);
            count++;
        }
    }

    return count;
}

/**
   Holds data for swprintf writer
*/
static struct
{
    int count;
    int max;
    wchar_t *pos;
}
sw_data;

/**
   Writers for string output
*/
static void sw_writer(wchar_t c)
{
    if (sw_data.count < sw_data.max)
        *(sw_data.pos++)=c;
    sw_data.count++;
}

int vswprintf(wchar_t *out, size_t n, const wchar_t *filter, va_list va)
{
    int written;

    sw_data.pos=out;
    sw_data.max=n;
    sw_data.count=0;
    written=vgwprintf(&sw_writer,
                      filter,
                      va);
    if (written < n)
    {
        *sw_data.pos = 0;
    }
    else
    {
        written=-1;
    }

    return written;
}

int swprintf(wchar_t *out, size_t n, const wchar_t *filter, ...)
{
    va_list va;
    int written;

    va_start(va, filter);
    written = vswprintf(out, n, filter, va);
    va_end(va);
    return written;
}

/**
   Holds auxiliary data for fwprintf and wprintf writer
*/
static FILE *fw_data;

static void fw_writer(wchar_t c)
{
    fputwc(c, fw_data);
}

/*
   Writers for file output
*/
int vfwprintf(FILE *f, const wchar_t *filter, va_list va)
{
    fw_data = f;
    return vgwprintf(&fw_writer, filter, va);
}

int fwprintf(FILE *f, const wchar_t *filter, ...)
{
    va_list va;
    int written;

    va_start(va, filter);
    written = vfwprintf(f, filter, va);
    va_end(va);
    return written;
}

int vwprintf(const wchar_t *filter, va_list va)
{
    return vfwprintf(stdout, filter, va);
}

int wprintf(const wchar_t *filter, ...)
{
    va_list va;
    int written;

    va_start(va, filter);
    written=vwprintf(filter, va);
    va_end(va);
    return written;
}

#endif

#ifndef HAVE_FGETWC
wint_t fgetwc(FILE *stream)
{
    wchar_t res;
    mbstate_t state = {};

    while (1)
    {
        int b = fgetc(stream);
        if (b == EOF)
        {
            return WEOF;
        }

        if (MB_CUR_MAX == 1) // single-byte locale, all values are legal
        {
            return b;
        }

        char bb = b;
        size_t sz = mbrtowc(&res, &bb, 1, &state);
        switch (sz)
        {
            case -1:
                return WEOF;
            case -2:
                break;
            case 0:
                return 0;
            default:
                return res;
        }
    }
}
#endif

#ifndef HAVE_FPUTWC
wint_t fputwc(wchar_t wc, FILE *stream)
{
    int res = 0;
    mbstate_t state = {};
    char s[MB_CUR_MAX + 1] = {};

    if (MB_CUR_MAX == 1) // single-byte locale (C/POSIX/ISO-8859)
    {
        // If `wc` contains a wide character we emit a question-mark.
        if (wc & ~0xFF)
        {
            wc = '?';
        }
        s[0] = (char)wc;
        res = fputs(s, stream);
    }
    else
    {
        size_t len = wcrtomb(s, wc, &state);
        if (len == (size_t)-1)
        {
            debug(1, L"Wide character %d has no narrow representation", wc);
        }
        else {
            res = fputs(s, stream);
        }
    }

    return res == EOF ? WEOF : wc;
}
#endif

#ifndef HAVE_WCSTOK

/*
  Used by fallback wcstok. Borrowed from glibc
*/
static size_t fish_wcsspn(const wchar_t *wcs,
                          const wchar_t *accept)
{
    register const wchar_t *p;
    register const wchar_t *a;
    register size_t count = 0;

    for (p = wcs; *p != L'\0'; ++p)
    {
        for (a = accept; *a != L'\0'; ++a)
            if (*p == *a)
                break;

        if (*a == L'\0')
            return count;
        else
            ++count;
    }
    return count;
}

/*
  Used by fallback wcstok. Borrowed from glibc
*/
static wchar_t *fish_wcspbrk(const wchar_t *wcs, const wchar_t *accept)
{
    while (*wcs != L'\0')
        if (wcschr(accept, *wcs) == NULL)
            ++wcs;
        else
            return (wchar_t *) wcs;
    return NULL;
}

/*
  Fallback wcstok implementation. Borrowed from glibc.
*/
wchar_t *wcstok(wchar_t *wcs, const wchar_t *delim, wchar_t **save_ptr)
{
    wchar_t *result;

    if (wcs == NULL)
    {
        if (*save_ptr == NULL)
        {
            errno = EINVAL;
            return NULL;
        }
        else
            wcs = *save_ptr;
    }

    /* Scan leading delimiters.  */
    wcs += fish_wcsspn(wcs, delim);

    if (*wcs == L'\0')
    {
        *save_ptr = NULL;
        return NULL;
    }

    /* Find the end of the token.  */
    result = wcs;

    wcs = fish_wcspbrk(result, delim);

    if (wcs == NULL)
    {
        /* This token finishes the string.  */
        *save_ptr = NULL;
    }
    else
    {
        /* Terminate the token and make *SAVE_PTR point past it.  */
        *wcs = L'\0';
        *save_ptr = wcs + 1;
    }
    return result;
}

#endif

/* Fallback implementations of wcsdup and wcscasecmp. On systems where these are not needed (e.g. building on Linux) these should end up just being stripped, as they are static functions that are not referenced in this file.
*/
__attribute__((unused))
static wchar_t *wcsdup_fallback(const wchar_t *in)
{
    size_t len=wcslen(in);
    wchar_t *out = (wchar_t *)malloc(sizeof(wchar_t)*(len+1));
    if (out == 0)
    {
        return 0;
    }

    memcpy(out, in, sizeof(wchar_t)*(len+1));
    return out;
}

__attribute__((unused))
static int wcscasecmp_fallback(const wchar_t *a, const wchar_t *b)
{
    if (*a == 0)
    {
        return (*b==0)?0:-1;
    }
    else if (*b == 0)
    {
        return 1;
    }
    int diff = towlower(*a)-towlower(*b);
    if (diff != 0)
        return diff;
    else
        return wcscasecmp_fallback(a+1,b+1);
}

__attribute__((unused))
static int wcsncasecmp_fallback(const wchar_t *a, const wchar_t *b, size_t count)
{
    if (count == 0)
        return 0;

    if (*a == 0)
    {
        return (*b==0)?0:-1;
    }
    else if (*b == 0)
    {
        return 1;
    }
    int diff = towlower(*a)-towlower(*b);
    if (diff != 0)
        return diff;
    else
        return wcsncasecmp_fallback(a+1,b+1, count-1);
}


#if __APPLE__ && __DARWIN_C_LEVEL >= 200809L
/* Note parens avoid the macro expansion */
wchar_t *wcsdup_use_weak(const wchar_t *a)
{
    if (&wcsdup != NULL)
        return (wcsdup)(a);
    return wcsdup_fallback(a);
}

int wcscasecmp_use_weak(const wchar_t *a, const wchar_t *b)
{
    if (&wcscasecmp != NULL)
        return (wcscasecmp)(a, b);
    return wcscasecmp_fallback(a, b);
}

int wcsncasecmp_use_weak(const wchar_t *s1, const wchar_t *s2, size_t n)
{
    if (&wcsncasecmp != NULL)
        return (wcsncasecmp)(s1, s2, n);
    return wcsncasecmp_fallback(s1, s2, n);
}

#else //__APPLE__

#ifndef HAVE_WCSDUP
wchar_t *wcsdup(const wchar_t *in)
{
    return wcsdup_fallback(in);

}
#endif


#ifndef HAVE_WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b)
{
    return wcscasecmp_fallback(a, b);
}
#endif
#endif //__APPLE__

#ifndef HAVE_WCSLEN
size_t wcslen(const wchar_t *in)
{
    const wchar_t *end=in;
    while (*end)
        end++;
    return end-in;
}
#endif

#ifndef HAVE_WCSNCASECMP
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t count)
{
    return wcsncasecmp_fallback(a, b, count);
}
#endif

#ifndef HAVE_WCWIDTH
int wcwidth(wchar_t c)
{
    if (c < 32)
        return 0;
    if (c == 127)
        return 0;
    return 1;
}
#endif

#ifndef HAVE_WCSNDUP
wchar_t *wcsndup(const wchar_t *in, size_t c)
{
    wchar_t *res = (wchar_t *)malloc(sizeof(wchar_t)*(c+1));
    if (res == 0)
    {
        return 0;
    }
    wcslcpy(res, in, c+1);
    return res;
}
#endif

long convert_digit(wchar_t d, int base)
{
    long res=-1;
    if ((d <= L'9') && (d >= L'0'))
    {
        res = d - L'0';
    }
    else if ((d <= L'z') && (d >= L'a'))
    {
        res = d + 10 - L'a';
    }
    else if ((d <= L'Z') && (d >= L'A'))
    {
        res = d + 10 - L'A';
    }
    if (res >= base)
    {
        res = -1;
    }

    return res;
}

#ifndef HAVE_WCSTOL
long wcstol(const wchar_t *nptr,
            wchar_t **endptr,
            int base)
{
    long long res=0;
    int is_set=0;
    if (base > 36)
    {
        errno = EINVAL;
        return 0;
    }

    while (1)
    {
        long nxt = convert_digit(*nptr, base);
        if (endptr != 0)
            *endptr = (wchar_t *)nptr;
        if (nxt < 0)
        {
            if (!is_set)
            {
                errno = EINVAL;
            }
            return res;
        }
        res = (res*base)+nxt;
        is_set = 1;
        if (res > LONG_MAX)
        {
            errno = ERANGE;
            return LONG_MAX;
        }
        if (res < LONG_MIN)
        {
            errno = ERANGE;
            return LONG_MIN;
        }
        nptr++;
    }
}

#endif
#ifndef HAVE_WCSLCAT

/*$OpenBSD: strlcat.c,v 1.11 2003/06/17 21:56:24 millert Exp $*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

size_t
wcslcat(wchar_t *dst, const wchar_t *src, size_t siz)
{

    register wchar_t *d = dst;
    register const wchar_t *s = src;
    register size_t n = siz;
    size_t dlen;

    /* Find the end of dst and adjust bytes left but don't go past end */
    while (n-- != 0 && *d != '\0')
        d++;

    dlen = d - dst;
    n = siz - dlen;

    if (n == 0)
        return(dlen + wcslen(s));

    while (*s != '\0')
    {
        if (n != 1)
        {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';

    return(dlen + (s - src));
    /* count does not include NUL */
}

#endif
#ifndef HAVE_WCSLCPY

/*$OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

size_t
wcslcpy(wchar_t *dst, const wchar_t *src, size_t siz)
{
    register wchar_t *d = dst;
    register const wchar_t *s = src;
    register size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0)
    {
        do
        {
            if ((*d++ = *s++) == 0)
                break;
        }
        while (--n != 0);
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0)
    {
        if (siz != 0)
            *d = '\0';
        /* NUL-terminate dst */
        while (*s++)
            ;
    }
    return(s - src - 1);
    /* count does not include NUL */
}

#endif

#ifndef HAVE_LRAND48_R

int lrand48_r(struct drand48_data *buffer, long int *result)
{
    *result = rand_r(&buffer->seed);
    return 0;
}

int srand48_r(long int seedval, struct drand48_data *buffer)
{
    buffer->seed = (unsigned int)seedval;
    return 0;
}

#endif

#ifndef HAVE_FUTIMES

int futimes(int fd, const struct timeval *times)
{
    errno = ENOSYS;
    return -1;
}

#endif

#if HAVE_GETTEXT

char * fish_gettext(const char * msgid)
{
    return gettext(msgid);;
}

char * fish_bindtextdomain(const char * domainname, const char * dirname)
{
    return bindtextdomain(domainname, dirname);
}

char * fish_textdomain(const char * domainname)
{
    return textdomain(domainname);
}

#else

char *fish_gettext(const char * msgid)
{
    return (char *)msgid;
}

char *fish_bindtextdomain(const char * domainname, const char * dirname)
{
    return NULL;
}

char *fish_textdomain(const char * domainname)
{
    return NULL;
}

#endif

#if HAVE_DCGETTEXT

char *fish_dcgettext(const char * domainname, const char * msgid, int category)
{
    return dcgettext(domainname, msgid, category);
}

#else

char *fish_dcgettext(const char * domainname, const char * msgid, int category)
{
    return (char *)msgid;
}


#endif

#ifndef HAVE__NL_MSG_CAT_CNTR

int _nl_msg_cat_cntr=0;

#endif

#ifndef HAVE_KILLPG
int killpg(int pgr, int sig)
{
    assert(pgr > 1);
    return kill(-pgr, sig);
}
#endif

#ifndef HAVE_BACKTRACE
int backtrace(void **buffer, int size)
{
    return 0;
}
#endif

#ifndef HAVE_BACKTRACE_SYMBOLS_FD
char ** backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
    return 0;
}
#endif

#ifndef HAVE_SYSCONF

long sysconf(int name)
{
    if (name == _SC_ARG_MAX)
    {
#ifdef ARG_MAX
        return ARG_MAX;
#endif
    }

    return -1;

}
#endif

#ifndef HAVE_NAN
double nan(char *tagp)
{
    return 0.0/0.0;
}
#endif

/* Big hack to use our versions of wcswidth where we know them to be broken, like on OS X */
#ifndef HAVE_BROKEN_WCWIDTH
#define HAVE_BROKEN_WCWIDTH 1
#endif

#if ! HAVE_BROKEN_WCWIDTH

int fish_wcwidth(wchar_t wc)
{
    return wcwidth(wc);
}

int fish_wcswidth(const wchar_t *str, size_t n)
{
    return wcswidth(str, n);
}

#else

static int mk_wcwidth(wchar_t wc);
static int mk_wcswidth(const wchar_t *pwcs, size_t n);

int fish_wcwidth(wchar_t wc)
{
    return mk_wcwidth(wc);
}

int fish_wcswidth(const wchar_t *str, size_t n)
{
    return mk_wcswidth(str, n);
}

/*
 * This is an implementation of wcwidth() and wcswidth() (defined in
 * IEEE Std 1002.1-2001) for Unicode.
 *
 * http://www.opengroup.org/onlinepubs/007904975/functions/wcwidth.html
 * http://www.opengroup.org/onlinepubs/007904975/functions/wcswidth.html
 *
 * In fixed-width output devices, Latin characters all occupy a single
 * "cell" position of equal width, whereas ideographic CJK characters
 * occupy two such cells. Interoperability between terminal-line
 * applications and (teletype-style) character terminals using the
 * UTF-8 encoding requires agreement on which character should advance
 * the cursor by how many cell positions. No established formal
 * standards exist at present on which Unicode character shall occupy
 * how many cell positions on character terminals. These routines are
 * a first attempt of defining such behavior based on simple rules
 * applied to data provided by the Unicode Consortium.
 *
 * For some graphical characters, the Unicode standard explicitly
 * defines a character-cell width via the definition of the East Asian
 * FullWidth (F), Wide (W), Half-width (H), and Narrow (Na) classes.
 * In all these cases, there is no ambiguity about which width a
 * terminal shall use. For characters in the East Asian Ambiguous (A)
 * class, the width choice depends purely on a preference of backward
 * compatibility with either historic CJK or Western practice.
 * Choosing single-width for these characters is easy to justify as
 * the appropriate long-term solution, as the CJK practice of
 * displaying these characters as double-width comes from historic
 * implementation simplicity (8-bit encoded characters were displayed
 * single-width and 16-bit ones double-width, even for Greek,
 * Cyrillic, etc.) and not any typographic considerations.
 *
 * Much less clear is the choice of width for the Not East Asian
 * (Neutral) class. Existing practice does not dictate a width for any
 * of these characters. It would nevertheless make sense
 * typographically to allocate two character cells to characters such
 * as for instance EM SPACE or VOLUME INTEGRAL, which cannot be
 * represented adequately with a single-width glyph. The following
 * routines at present merely assign a single-cell width to all
 * neutral characters, in the interest of simplicity. This is not
 * entirely satisfactory and should be reconsidered before
 * establishing a formal standard in this area. At the moment, the
 * decision which Not East Asian (Neutral) characters should be
 * represented by double-width glyphs cannot yet be answered by
 * applying a simple rule from the Unicode database content. Setting
 * up a proper standard for the behavior of UTF-8 character terminals
 * will require a careful analysis not only of each Unicode character,
 * but also of each presentation form, something the author of these
 * routines has avoided to do so far.
 *
 * http://www.unicode.org/unicode/reports/tr11/
 *
 * Markus Kuhn -- 2007-05-26 (Unicode 5.0)
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version: http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

struct interval
{
    int first;
    int last;
};

/* auxiliary function for binary search in interval table */
static int bisearch(wchar_t ucs, const struct interval *table, int max)
{
    int min = 0;

    if (ucs < table[0].first || ucs > table[max].last)
        return 0;
    while (max >= min)
    {
        int mid = (min + max) / 2;
        if (ucs > table[mid].last)
            min = mid + 1;
        else if (ucs < table[mid].first)
            max = mid - 1;
        else
            return 1;
    }

    return 0;
}


/* The following two functions define the column width of an ISO 10646
 * character as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - SOFT HYPHEN (U+00AD) has a column width of 1.
 *
 *    - Other format characters (general category code Cf in the Unicode
 *      database) and ZERO WIDTH SPACE (U+200B) have a column width of 0.
 *
 *    - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *      have a column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      Full-width (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

static int mk_wcwidth(wchar_t ucs)
{
    /* sorted list of non-overlapping intervals of non-spacing characters */
    /* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c" */
    static const struct interval combining[] =
    {
        { 0x0300, 0x036F }, { 0x0483, 0x0486 }, { 0x0488, 0x0489 },
        { 0x0591, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
        { 0x05C4, 0x05C5 }, { 0x05C7, 0x05C7 }, { 0x0600, 0x0603 },
        { 0x0610, 0x0615 }, { 0x064B, 0x065E }, { 0x0670, 0x0670 },
        { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
        { 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
        { 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 }, { 0x0901, 0x0902 },
        { 0x093C, 0x093C }, { 0x0941, 0x0948 }, { 0x094D, 0x094D },
        { 0x0951, 0x0954 }, { 0x0962, 0x0963 }, { 0x0981, 0x0981 },
        { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD },
        { 0x09E2, 0x09E3 }, { 0x0A01, 0x0A02 }, { 0x0A3C, 0x0A3C },
        { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D },
        { 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC },
        { 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD },
        { 0x0AE2, 0x0AE3 }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
        { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
        { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
        { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
        { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBC, 0x0CBC },
        { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
        { 0x0CE2, 0x0CE3 }, { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D },
        { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 },
        { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E },
        { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC },
        { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
        { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
        { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 },
        { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 },
        { 0x1032, 0x1032 }, { 0x1036, 0x1037 }, { 0x1039, 0x1039 },
        { 0x1058, 0x1059 }, { 0x1160, 0x11FF }, { 0x135F, 0x135F },
        { 0x1712, 0x1714 }, { 0x1732, 0x1734 }, { 0x1752, 0x1753 },
        { 0x1772, 0x1773 }, { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD },
        { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD },
        { 0x180B, 0x180D }, { 0x18A9, 0x18A9 }, { 0x1920, 0x1922 },
        { 0x1927, 0x1928 }, { 0x1932, 0x1932 }, { 0x1939, 0x193B },
        { 0x1A17, 0x1A18 }, { 0x1B00, 0x1B03 }, { 0x1B34, 0x1B34 },
        { 0x1B36, 0x1B3A }, { 0x1B3C, 0x1B3C }, { 0x1B42, 0x1B42 },
        { 0x1B6B, 0x1B73 }, { 0x1DC0, 0x1DCA }, { 0x1DFE, 0x1DFF },
        { 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x2063 },
        { 0x206A, 0x206F }, { 0x20D0, 0x20EF }, { 0x302A, 0x302F },
        { 0x3099, 0x309A }, { 0xA806, 0xA806 }, { 0xA80B, 0xA80B },
        { 0xA825, 0xA826 }, { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F },
        { 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB },
        { 0x10A01, 0x10A03 }, { 0x10A05, 0x10A06 }, { 0x10A0C, 0x10A0F },
        { 0x10A38, 0x10A3A }, { 0x10A3F, 0x10A3F }, { 0x1D167, 0x1D169 },
        { 0x1D173, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
        { 0x1D242, 0x1D244 }, { 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F },
        { 0xE0100, 0xE01EF }
    };

    /* test for 8-bit control characters */
    if (ucs == 0)
        return 0;
    if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
        return -1;

    /* binary search in table of non-spacing characters */
    if (bisearch(ucs, combining,
                 sizeof(combining) / sizeof(struct interval) - 1))
        return 0;

    /* if we arrive here, ucs is not a combining or C0/C1 control character */

    return 1 +
           (ucs >= 0x1100 &&
            (ucs <= 0x115f ||                    /* Hangul Jamo init. consonants */
             ucs == 0x2329 || ucs == 0x232a ||
             (ucs >= 0x2e80 && ucs <= 0xa4cf &&
              ucs != 0x303f) ||                  /* CJK ... Yi */
             (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
             (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
             (ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
             (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
             (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
             (ucs >= 0xffe0 && ucs <= 0xffe6) ||
             (ucs >= 0x20000 && ucs <= 0x2fffd) ||
             (ucs >= 0x30000 && ucs <= 0x3fffd)));
}

static int mk_wcswidth(const wchar_t *pwcs, size_t n)
{
    int width = 0;
    for (size_t i=0; i < n; i++)
    {
        if (pwcs[i] == L'\0')
            break;

        int w = mk_wcwidth(pwcs[i]);
        if (w < 0)
        {
            width = -1;
            break;
        }
        width += w;
    }
    return width;
}

#endif // HAVE_BROKEN_WCWIDTH
