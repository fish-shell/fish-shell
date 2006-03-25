
#ifndef FISH_FALLBACK_H
#define FISH_FALLBACK_H

#include <stdio.h>
#include <stdarg.h>
#include <wctype.h>
#include <wchar.h>
#include "common.h"

/**
   Under curses, tputs expects an int (*func)(char) as its last
   parameter, but in ncurses, tputs expects a int (*func)(int) as its
   last parameter. tputs_arg_t is defined to always be what tputs
   expects. Hopefully.
*/

#ifdef NCURSES_VERSION
typedef int tputs_arg_t;
#else
typedef char tputs_arg_t;
#endif

#ifdef TPUTS_KLUDGE

/**
   Linux on PPC seems to have a tputs implementation that sometimes
   behaves strangely. This fallback seems to fix things.
*/
int tputs(const char *str, int affcnt, int (*putc)(tputs_arg_t));

#endif

#ifndef HAVE_FWPRINTF

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we implement our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int fwprintf( FILE *f, const wchar_t *format, ... );


/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int swprintf( wchar_t *str, size_t l, const wchar_t *format, ... );

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int wprintf( const wchar_t *format, ... );

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int vwprintf( const wchar_t *filter, va_list va );

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int vfwprintf( FILE *f, const wchar_t *filter, va_list va );

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int vswprintf( wchar_t *out, size_t n, const wchar_t *filter, va_list va );

#endif

#ifndef HAVE_FGETWC
/**
   Fallback implementation of fgetwc
*/
wint_t fgetwc(FILE *stream);

/**
   Fallback implementation of getwc
*/
wint_t getwc(FILE *stream);

#endif

#ifndef HAVE_FPUTWC

/**
   Fallback implementation of fputwc
*/
wint_t fputwc(wchar_t wc, FILE *stream);
/**
   Fallback implementation of putwc
*/
wint_t putwc(wchar_t wc, FILE *stream);

#endif

#ifndef HAVE_WCSTOK

/**
   Fallback implementation of wcstok. Uses code borrowed from glibc.
*/
wchar_t *wcstok(wchar_t *wcs, const wchar_t *delim, wchar_t **ptr);

#endif

#ifndef HAVE_WCWIDTH

/**
   Return the number of columns used by a character. This is a libc
   function, but the prototype for this function is missing in some libc
   implementations. 

   Fish has a fallback implementation in case the implementation is
   missing altogether.  In locales without a native wcwidth, Unicode
   is probably so broken that it isn't worth trying to implement a
   real wcwidth. Therefore, the fallback wcwidth assumes any printing
   character takes up one column and anything else uses 0 columns.
*/
int wcwidth( wchar_t c );

#endif

#ifndef HAVE_WCSDUP

/**
   Create a duplicate string. Wide string version of strdup. Will
   automatically exit if out of memory.
*/
wchar_t *wcsdup(const wchar_t *in);

#endif

#ifndef HAVE_WCSLEN

/**
   Fallback for wcsen. Returns the length of the specified string.
*/
size_t wcslen(const wchar_t *in);

#endif

#ifndef HAVE_WCSCASECMP
/**
   Case insensitive string compare function. Wide string version of
   strcasecmp.

   This implementation of wcscasecmp does not take into account
   esoteric locales where uppercase and lowercase do not cleanly
   transform between each other. Hopefully this should be fine since
   fish only uses this function with one of the strings supplied by
   fish and guaranteed to be a sane, english word. Using wcscasecmp on
   a user-supplied string should be considered a bug.
*/
int wcscasecmp( const wchar_t *a, const wchar_t *b );

#endif

#ifndef HAVE_WCSNCASECMP

/**
   Case insensitive string compare function. Wide string version of
   strncasecmp.

   This implementation of wcsncasecmp does not take into account
   esoteric locales where uppercase and lowercase do not cleanly
   transform between each other. Hopefully this should be fine since
   fish only uses this function with one of the strings supplied by
   fish and guaranteed to be a sane, english word. Using wcsncasecmp on
   a user-supplied string should be considered a bug.
*/
int wcsncasecmp( const wchar_t *a, const wchar_t *b, int count );

/**
   Returns a newly allocated wide character string wich is a copy of
   the string in, but of length c or shorter. The returned string is
   always null terminated, and the null is not included in the string
   length.
*/

#endif

#ifndef HAVE_WCSNDUP

/**
   Fallback for wcsndup function. Returns a copy of \c in, truncated
   to a maximum length of \c c.
*/
wchar_t *wcsndup( const wchar_t *in, int c );

#endif

/**
   Converts from wide char to digit in the specified base. If d is not
   a valid digit in the specified base, return -1.
*/
long convert_digit( wchar_t d, int base );

#ifndef HAVE_WCSTOL

/**
   Fallback implementation. Convert a wide character string to a
   number in the specified base. This functions is the wide character
   string equivalent of strtol. For bases of 10 or lower, 0..9 are
   used to represent numbers. For bases below 36, a-z and A-Z are used
   to represent numbers higher than 9. Higher bases than 36 are not
   supported.
*/
long wcstol(const wchar_t *nptr,
			wchar_t **endptr,
			int base);

#endif

#endif
