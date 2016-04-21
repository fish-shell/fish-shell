#ifndef FISH_FALLBACK_H
#define FISH_FALLBACK_H

#include "config.h"

// IWYU likes to recommend adding <term.h> when we want <ncurses.h>. If we add <term.h> it breaks
// compiling several modules that include this header because they use symbols which are defined as
// macros in <term.h>.
// IWYU pragma: no_include <term.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
// The following include must be kept despite what IWYU says. That's because of the interaction
// between the weak linking of `wcsdup` and `wcscasecmp` via `#define`s below and the declarations
// in <wchar.h>. At least on OS X if we don't do this we get compilation errors do to the macro
// substitution if wchar.h is included after this header.
#include <wchar.h>  // IWYU pragma: keep
#if HAVE_NCURSES_H
#include <ncurses.h>  // IWYU pragma: keep
#endif

/** fish's internal versions of wcwidth and wcswidth, which can use an internal implementation if the system one is busted. */
int fish_wcwidth(wchar_t wc);
int fish_wcswidth(const wchar_t *str, size_t n);

#ifndef WCHAR_MAX
/**
   This _should_ be defined by wchar.h, but e.g. OpenBSD doesn't.
*/
#define WCHAR_MAX INT_MAX
#endif

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

#ifndef SIGIO
#define SIGIO SIGUSR1
#endif

#ifndef SIGWINCH
#define SIGWINCH SIGUSR2
#endif

#ifndef HAVE_WINSIZE
/**
   Structure used to get the size of a terminal window
 */
struct winsize
{
    /**
       Number of rows
     */
    unsigned short ws_row;
    /**
       Number of columns
     */
    unsigned short ws_col;
}
;

#endif

#ifdef TPUTS_KLUDGE

/**
   Linux on PPC seems to have a tputs implementation that sometimes
   behaves strangely. This fallback seems to fix things.
*/
int tputs(const char *str, int affcnt, int (*fish_putc)(tputs_arg_t));

#endif

#ifdef TPARM_SOLARIS_KLUDGE

/**
   Solaris tparm has a set fixed of paramters in it's curses implementation,
   work around this here.
*/

#define tparm tparm_solaris_kludge
char *tparm_solaris_kludge(char *str, ...);

#endif

#ifndef HAVE_FWPRINTF

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we implement our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int fwprintf(FILE *f, const wchar_t *format, ...);


/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int swprintf(wchar_t *str, size_t l, const wchar_t *format, ...);

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int wprintf(const wchar_t *format, ...);

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int vwprintf(const wchar_t *filter, va_list va);

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int vfwprintf(FILE *f, const wchar_t *filter, va_list va);

/**
   Print formated string. Some operating systems (Like NetBSD) do not
   have wide string formating functions.  Therefore we define our
   own. Not at all complete. Supports wide and narrow characters,
   strings and decimal numbers, position (%n), field width and
   precision.
*/
int vswprintf(wchar_t *out, size_t n, const wchar_t *filter, va_list va);

#endif

#ifndef HAVE_FGETWC
// Fallback implementation of fgetwc.
wint_t fgetwc(FILE *stream);
#endif

#ifndef HAVE_FPUTWC
// Fallback implementation of fputwc.
wint_t fputwc(wchar_t wc, FILE *stream);
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
int wcwidth(wchar_t c);

#endif


/** On OS X, use weak linking for wcsdup and wcscasecmp. Weak linking allows you to call the function only if it exists at runtime. You can detect it by testing the function pointer against NULL. To avoid making the callers do that, redefine wcsdup to wcsdup_use_weak, and likewise with wcscasecmp. This lets us use the same binary on SnowLeopard (10.6) and Lion+ (10.7), even though these functions only exist on 10.7+.

    On other platforms, use what's detected at build time.
*/
#if __APPLE__ && __DARWIN_C_LEVEL >= 200809L
wchar_t *wcsdup_use_weak(const wchar_t *);
int wcscasecmp_use_weak(const wchar_t *, const wchar_t *);
int wcsncasecmp_use_weak(const wchar_t *s1, const wchar_t *s2, size_t n);
#define wcsdup(a) wcsdup_use_weak((a))
#define wcscasecmp(a, b) wcscasecmp_use_weak((a), (b))
#define wcsncasecmp(a, b, c) wcsncasecmp_use_weak((a), (b), (c))

#else

#ifndef HAVE_WCSDUP

/**
   Create a duplicate string. Wide string version of strdup. Will
   automatically exit if out of memory.
*/
wchar_t *wcsdup(const wchar_t *in);

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
int wcscasecmp(const wchar_t *a, const wchar_t *b);

#endif
#endif //__APPLE__


#ifndef HAVE_WCSLEN

/**
   Fallback for wclsen. Returns the length of the specified string.
*/
size_t wcslen(const wchar_t *in);

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
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t count);

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
wchar_t *wcsndup(const wchar_t *in, size_t c);

#endif

/**
   Converts from wide char to digit in the specified base. If d is not
   a valid digit in the specified base, return -1. This is a helper
   function for wcstol, but it is useful itself, so it is exported.
*/
long convert_digit(wchar_t d, int base);

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
#ifndef HAVE_WCSLCAT

/**
   Appends src to string dst of size siz (unlike wcsncat, siz is the
   full size of dst, not space left).  At most siz-1 characters will be
   copied.  Always NUL terminates (unless siz <= wcslen(dst)).  Returns
   wcslen(src) + MIN(siz, wcslen(initial dst)).  If retval >= siz,
   truncation occurred.

   This is the OpenBSD strlcat function, modified for wide characters,
   and renamed to reflect this change.

*/
size_t wcslcat(wchar_t *dst, const wchar_t *src, size_t siz);

#endif
#ifndef HAVE_WCSLCPY

/**
   Copy src to string dst of size siz.  At most siz-1 characters will
   be copied.  Always NUL terminates (unless siz == 0).  Returns
   wcslen(src); if retval >= siz, truncation occurred.

   This is the OpenBSD strlcpy function, modified for wide characters,
   and renamed to reflect this change.
*/
size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t siz);

#endif

#ifndef HAVE_LRAND48_R

/**
   Datastructure for the lrand48_r fallback implementation.
*/
struct drand48_data
{
    /**
       Seed value
    */
    unsigned int seed;
}
;

/**
   Fallback implementation of lrand48_r. Internally uses rand_r, so it is pretty weak.
*/
int lrand48_r(struct drand48_data *buffer, long int *result);

/**
   Fallback implementation of srand48_r, the seed function for lrand48_r.
*/
int srand48_r(long int seedval, struct drand48_data *buffer);

#endif

#ifndef HAVE_FUTIMES

int futimes(int fd, const struct timeval *times);

#endif

/* autoconf may fail to detect gettext (645), so don't define a function call gettext or we'll get build errors */

/** Cover for gettext() */
char * fish_gettext(const char * msgid);

/** Cover for bindtextdomain() */
char * fish_bindtextdomain(const char * domainname, const char * dirname);

/** Cover for textdomain() */
char * fish_textdomain(const char * domainname);

/* Cover for dcgettext */
char * fish_dcgettext(const char * domainname, const char * msgid, int category);

#ifndef HAVE__NL_MSG_CAT_CNTR

/**
   Some gettext implementation use have this variable, and by
   increasing it, one can tell the system that the translations need
   to be reloaded.
*/
extern int _nl_msg_cat_cntr;

#endif


#ifndef HAVE_KILLPG
/**
   Send specified signal to specified process group.
 */
int killpg(int pgr, int sig);
#endif

#ifndef HAVE_SYSCONF

#define _SC_ARG_MAX 1
long sysconf(int name);

#endif

#ifndef HAVE_NAN
double nan(char *tagp);
#endif


#endif
