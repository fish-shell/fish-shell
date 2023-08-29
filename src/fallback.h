#ifndef FISH_FALLBACK_H
#define FISH_FALLBACK_H

#include <stdint.h>
#include "config.h"

// The following include must be kept despite what IWYU says. That's because of the interaction
// between the weak linking of `wcscasecmp` via `#define`s below and the declarations
// in <wchar.h>. At least on OS X if we don't do this we get compilation errors do to the macro
// substitution if wchar.h is included after this header.
#include <cwchar>  // IWYU pragma: keep
                   //
// Width of ambiguous characters. 1 is typical default.
extern int32_t FISH_AMBIGUOUS_WIDTH;

// Width of emoji characters.
// 1 is the typical emoji width in Unicode 8.
extern int32_t FISH_EMOJI_WIDTH;


/// fish's internal versions of wcwidth and wcswidth, which can use an internal implementation if
/// the system one is busted.
int fish_wcwidth(wchar_t wc);
int fish_wcswidth(const wchar_t *str, size_t n);

// Replacement for mkostemp(str, O_CLOEXEC)
// This uses mkostemp if available,
// otherwise it uses mkstemp followed by fcntl
int fish_mkstemp_cloexec(char *);

/// thread_local support.
#if HAVE_CX11_THREAD_LOCAL
#define FISH_THREAD_LOCAL thread_local
#elif defined(__GNUC__)
#define FISH_THREAD_LOCAL __thread
#elif defined(_MSC_VER)
#define FISH_THREAD_LOCAL __declspec(thread)
#else  // !C++11 && !__GNUC__ && !_MSC_VER
#error "No known thread local storage qualifier for this platform"
#endif

#ifndef WCHAR_MAX
/// This _should_ be defined by wchar.h, but e.g. OpenBSD doesn't.
#define WCHAR_MAX INT_MAX
#endif

/// Both ncurses and NetBSD curses expect an int (*func)(int) as the last parameter for tputs.
/// Apparently OpenIndiana's curses still uses int (*func)(char) here.
#if TPUTS_USES_INT_ARG
using tputs_arg_t = int;
#else
using tputs_arg_t = char;
#endif

#ifndef HAVE_WINSIZE
/// Structure used to get the size of a terminal window.
struct winsize {
    /// Number of rows.
    unsigned short ws_row;
    /// Number of columns.
    unsigned short ws_col;
};

#endif

#if defined(TPARM_SOLARIS_KLUDGE)
/// Solaris tparm has a set fixed of parameters in its curses implementation, work around this here.
#define fish_tparm tparm_solaris_kludge
char *tparm_solaris_kludge(char *str, long p1 = 0, long p2 = 0, long p3 = 0, long p4 = 0,
                           long p5 = 0, long p6 = 0, long p7 = 0, long p8 = 0, long p9 = 0);
#else
#define fish_tparm tparm
#endif

/// These functions are missing from Solaris 10, and only accessible from
/// Solaris 11 in the std:: namespace.
#ifndef HAVE_WCSCASECMP
#ifdef HAVE_STD__WCSCASECMP
using std::wcscasecmp;
#else
int wcscasecmp(const wchar_t *a, const wchar_t *b);
#endif  // HAVE_STD__WCSCASECMP
#endif  // HAVE_WCSCASECMP

#ifndef HAVE_WCSNCASECMP
#ifdef HAVE_STD__WCSNCASECMP
using std::wcsncasecmp;
#else
int wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n);
#endif  // HAVE_STD__WCSNCASECMP
#endif  // HAVE_WCSNCASECMP

#ifndef HAVE_DIRFD
#ifndef __XOPEN_OR_POSIX
#define dirfd(d) (d->dd_fd)
#else
#define dirfd(d) (d->d_fd)
#endif
#endif

// autoconf may fail to detect gettext (645), so don't define a function call gettext or we'll get
// build errors.

/// Cover for gettext().
char *fish_gettext(const char *msgid);

/// Cover for bindtextdomain().
char *fish_bindtextdomain(const char *domainname, const char *dirname);

/// Cover for textdomain().
char *fish_textdomain(const char *domainname);

#ifndef HAVE_KILLPG
/// Send specified signal to specified process group.
int killpg(int pgr, int sig);
#endif

#ifndef HAVE_FLOCK
/// Fallback implementation of flock in terms of fcntl.
/// Danger! The semantics of flock and fcntl locking are very different.
/// Use with caution.
int flock(int fd, int op);

#define LOCK_SH 1  // Shared lock.
#define LOCK_EX 2  // Exclusive lock.
#define LOCK_UN 8  // Unlock.
#define LOCK_NB 4  // Don't block when locking.
#endif

#endif  // FISH_FALLBACK_H
