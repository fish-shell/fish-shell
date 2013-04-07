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
#include <string>
#include <utility>
#include "common.h"

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
   Wide character version of fopen(). This sets CLO_EXEC.
*/
FILE *wfopen(const wcstring &path, const char *mode);

/** Sets CLO_EXEC on a given fd */
bool set_cloexec(int fd);

/**
   Wide character version of freopen().
*/
FILE *wfreopen(const wcstring &path, const char *mode, FILE *stream);

/** Wide character version of open(). */
int wopen(const wcstring &pathname, int flags, mode_t mode = 0);

/** Wide character version of open() that also sets the close-on-exec flag (atomically when possible). */
int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode = 0);

/** Mark an fd as nonblocking; returns errno or 0 on success */
int make_fd_nonblocking(int fd);

/** Mark an fd as blocking; returns errno or 0 on success */
int make_fd_blocking(int fd);

/** Wide character version of creat(). */
int wcreat(const wcstring &pathname, mode_t mode);


/** Wide character version of opendir(). Note that opendir() is guaranteed to set close-on-exec by POSIX (hooray). */
DIR *wopendir(const wcstring &name);

/**
   Wide character version of stat().
*/
int wstat(const wcstring &file_name, struct stat *buf);

/**
   Wide character version of lstat().
*/
int lwstat(const wcstring &file_name, struct stat *buf);

/**
   Wide character version of access().
*/
int waccess(const wcstring &pathname, int mode);

/**
   Wide character version of unlink().
*/
int wunlink(const wcstring &pathname);

/**
   Wide character version of perror().
*/
void wperror(const wcstring &s);

/**
  Async-safe version of perror().
*/
void safe_perror(const char *message);

/**
  Async-safe version of strerror().
*/
const char *safe_strerror(int err);

/**
   Wide character version of getcwd().
*/
wchar_t *wgetcwd(wchar_t *buff, size_t sz);

/**
   Wide character version of chdir()
*/
int wchdir(const wcstring &dir);

/**
  Wide character version of realpath function. Just like the GNU
  version of realpath, wrealpath will accept 0 as the value for the
  second argument, in which case the result will be allocated using
  malloc, and must be free'd by the user.
*/
wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path);

/**
   Wide character version of readdir()
*/
bool wreaddir(DIR *dir, std::wstring &out_name);
bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, std::wstring &out_name, bool *out_is_dir);

/**
   Wide character version of dirname()
*/
std::wstring wdirname(const std::wstring &path);

/**
   Wide character version of basename()
*/
std::wstring wbasename(const std::wstring &path);

/**
   Wide character wrapper around the gettext function. For historic
   reasons, unlike the real gettext function, wgettext takes care of
   setting the correct domain, etc. using the textdomain and
   bindtextdomain functions. This should probably be moved out of
   wgettext, so that wgettext will be nothing more than a wrapper
   around gettext, like all other functions in this file.
*/
const wchar_t *wgettext(const wchar_t *in);

/**
   Wide character version of getenv
*/
const wchar_t *wgetenv(const wcstring &name);

/**
   Wide character version of mkdir
*/
int wmkdir(const wcstring &dir, int mode);

/**
   Wide character version of rename
*/
int wrename(const wcstring &oldName, const wcstring &newName);

/** Like wcstol(), but fails on a value outside the range of an int */
int fish_wcstoi(const wchar_t *str, wchar_t ** endptr, int base);

/** Class for representing a file's inode. We use this to detect and avoid symlink loops, among other things. */
typedef std::pair<dev_t, ino_t> file_id_t;


#endif
