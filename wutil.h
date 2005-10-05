/** \file wutil.h

  Prototypes for wide character equivalents of various standard unix
  functions. 

*/
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include <wchar.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


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


#if !HAVE_WPRINTF

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


#endif

#endif
