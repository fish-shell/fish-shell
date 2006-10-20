/** \file wutil.h

  Prototypes for wide character equivalents of various standard unix
  functions. 
*/
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>

/**
   Wide version of the dirent data structure
*/
struct wdirent
{
	/**
	   The name of the current directory
	*/
	wchar_t *d_name;
}
	;


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

/** 
	Wide character version of realpath function. Just like the GNU
	version of realpath, wrealpath will accept 0 as the value for the
	second argument, in which case the result will be allocated using
	malloc, and must be free'd by the user.
*/
wchar_t *wrealpath(const wchar_t *pathname, wchar_t *resolved_path);

/**
   Wide character version of readdir()
*/
struct wdirent *wreaddir(DIR *dir );

/**
   Wide character version of dirname()
*/
wchar_t *wdirname( wchar_t *path );

/**
   Wide character version of basename()
*/
wchar_t *wbasename( const wchar_t *path );

/**
   Wide character wrapper around the gettext function. For historic
   reasons, unlike the real gettext function, wgettext takes care of
   setting the correct domain, etc. using the textdomain and
   bindtextdomain functions. This should probably be moved out of
   wgettext, so that wgettext will be nothing more than a wrapper
   around gettext, like all other functions in this file.
*/
const wchar_t *wgettext( const wchar_t *in );

/**
   Wide character version of getenv
*/
wchar_t *wgetenv( const wchar_t *name );

/**
   Wide character version of mkdir
*/
int wmkdir( const wchar_t *dir, int mode );

/**
   Wide character version of rename
*/
int wrename( const wchar_t *old, const wchar_t *new );

#endif
