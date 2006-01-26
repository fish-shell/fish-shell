/** \file wutil.h

  Prototypes for wide character equivalents of various standard unix
  functions. Also contains fallback implementations of a large number
  of wide character unix functions.
*/
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include <wchar.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>

/**
   Call this function on startup to create internal wutil
   resources. This function doesn't do anything.
*/
void wutil_init();

/**
   Call this function on exit to free internal wutil resources
*/
void wutil_destroy();

/**
   Wide character version of fopen().
*/
FILE *wfopen(const wchar_t *path, const char *mode);

/**
   Wide character version of freopen().
*/
FILE *wfreopen(const wchar_t *path, const char *mode, FILE *stream);

/**
   Wide character version of open().
*/
int wopen(const wchar_t *pathname, int flags, ...);

/**
   Wide character version of creat().
*/
int wcreat(const wchar_t *pathname, mode_t mode);


/**
   Wide character version of opendir().
*/
DIR *wopendir(const wchar_t *name);

/**
   Wide character version of stat().
*/
int wstat(const wchar_t *file_name, struct stat *buf);

/**
   Wide character version of lstat().
*/
int lwstat(const wchar_t *file_name, struct stat *buf);

/**
   Wide character version of access().
*/
int waccess(const wchar_t *pathname, int mode);

/**
   Wide character version of perror().
*/
void wperror(const wchar_t *s);

/**
   Wide character version of getcwd().
*/
wchar_t *wgetcwd( wchar_t *buff, size_t sz );

/**
   Wide character version of chdir()
*/
int wchdir( const wchar_t * dir );

/*
  Here follows the prototypes for fallback implementations of various
  standarcs libc functions relating to wide character support. Some of
  these prototypes are always defined, since some libc versions
  include the code, but you have to use special magical #defines for
  the prototype to appear.
*/

#if !HAVE_FWPRINTF

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

/**
   Create a duplicate string. Wide string version of strdup. Will
   automatically exit if out of memory.
*/
wchar_t *wcsdup(const wchar_t *in);

size_t wcslen(const wchar_t *in);

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
wchar_t *wcsndup( const wchar_t *in, int c );

/**
   Converts from wide char to digit in the specified base. If d is not
   a valid digit in the specified base, return -1.
*/
long convert_digit( wchar_t d, int base );

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
