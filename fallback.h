
#ifndef FISH_FALLBACK_H
#define FISH_FALLBACK_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <wctype.h>
#include <wchar.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#ifndef WCHAR_MAX
/**
   This _should_ be defined by wchar.h, but e.g. OpenBSD doesn't.
*/
#define WCHAR_MAX INT_MAX
#endif

/**
   Make sure __func__ is defined to some string. In C99, this should
   be the currently compiled function. If we aren't using C99 or
   later, older versions of GCC had __FUNCTION__.
*/
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
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
char *tparm_solaris_kludge( char *str, ... );

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
   a valid digit in the specified base, return -1. This is a helper
   function for wcstol, but it is useful itself, so it is exported.
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
size_t wcslcat( wchar_t *dst, const wchar_t *src, size_t siz );

#endif
#ifndef HAVE_WCSLCPY

/**
   Copy src to string dst of size siz.  At most siz-1 characters will
   be copied.  Always NUL terminates (unless siz == 0).  Returns
   wcslen(src); if retval >= siz, truncation occurred.

   This is the OpenBSD strlcpy function, modified for wide characters,
   and renamed to reflect this change. 
*/
size_t wcslcpy( wchar_t *dst, const wchar_t *src, size_t siz );

#endif

#ifdef HAVE_BROKEN_DEL_CURTERM

/**
   BSD del_curterm seems to do a double-free. We redefine it as a no-op
*/
#define del_curterm(oterm) OK
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
int lrand48_r( struct drand48_data *buffer, long int *result );

/**
   Fallback implementation of srand48_r, the seed function for lrand48_r.
*/
int srand48_r( long int seedval, struct drand48_data *buffer );

#endif

#ifndef HAVE_FUTIMES

int futimes( int fd, const struct timeval *times );

#endif

#ifndef HAVE_GETTEXT

/**
   Fallback implementation of gettext. Just returns the original string.
*/
char * gettext( const char * msgid );

/**
   Fallback implementation of bindtextdomain. Does nothing.
*/
char * bindtextdomain( const char * domainname, const char * dirname );

/**
   Fallback implementation of textdomain. Does nothing.
*/
char * textdomain( const char * domainname );

#endif

#ifndef HAVE_DCGETTEXT

/**
   Fallback implementation of dcgettext. Just returns the original string.
*/
char * dcgettext ( const char * domainname, 
		   const char * msgid,
		   int category );

#endif

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
int killpg( int pgr, int sig );
#endif


#ifndef HAVE_WORKING_GETOPT_LONG

/**
   Struct describing a long getopt option
 */
struct option 
{
	/**
	   Name of option
	 */
	const char *name;
	/**
	   Flag
	 */
	int has_arg;
	/**
	   Flag
	 */
	int *flag;
	/**
	   Return value
	 */
	int val;	
}
;

#ifndef no_argument
#define	no_argument 0
#endif

#ifndef required_argument
#define	required_argument 1
#endif

#ifndef optional_argument
#define	optional_argument 2
#endif

int getopt_long(int argc, 
		char * const argv[],
		const char *optstring,
		const struct option *longopts, 
		int *longindex);

#endif

#ifndef HAVE_SYSCONF

#define _SC_ARG_MAX 1
long sysconf(int name);

#endif

#ifndef HAVE_NAN
double nan(char *tagp);
#endif


#endif
