#ifndef FISH_FALLBACK_H
#define FISH_FALLBACK_H

#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
// The following include must be kept despite what IWYU says. That's because of the interaction
// between the weak linking of `wcsdup` and `wcscasecmp` via `#define`s below and the declarations
// in <wchar.h>. At least on OS X if we don't do this we get compilation errors do to the macro
// substitution if wchar.h is included after this header.
#include <wchar.h>  // IWYU pragma: keep

/// The column width of ambiguous East Asian characters.
extern int g_fish_ambiguous_width;

/// The column width of emoji characters. This must be configurable because the value changed
/// between Unicode 8 and Unicode 9, wcwidth() is emoji-ignorant, and terminal emulators do
/// different things. See issues like #4539 and https://github.com/neovim/neovim/issues/4976 for how
/// painful this is. A value of 0 means to use the guessed value.
extern int g_fish_emoji_width;

/// The guessed value of the emoji width based on TERM.
extern int g_guessed_fish_emoji_width;

/// fish's internal versions of wcwidth and wcswidth, which can use an internal implementation if
/// the system one is busted.
int fish_wcwidth(wchar_t wc);
int fish_wcswidth(const wchar_t *str, size_t n);

// Replacement for mkostemp(str, O_CLOEXEC)
// This uses mkostemp if available,
// otherwise it uses mkstemp followed by fcntl
int fish_mkstemp_cloexec(char *);

#ifndef WCHAR_MAX
/// This _should_ be defined by wchar.h, but e.g. OpenBSD doesn't.
#define WCHAR_MAX INT_MAX
#endif

/// Under curses, tputs expects an int (*func)(char) as its last parameter, but in ncurses, tputs
/// expects a int (*func)(int) as its last parameter. tputs_arg_t is defined to always be what tputs
/// expects. Hopefully.
#if defined(NCURSES_VERSION) || defined(__NetBSD__)
typedef int tputs_arg_t;
#else
typedef char tputs_arg_t;
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
/// Solaris tparm has a set fixed of paramters in its curses implementation, work around this here.
#define tparm tparm_solaris_kludge
char *tparm_solaris_kludge(char *str, long p1 = 0, long p2 = 0, long p3 = 0, long p4 = 0,
                           long p5 = 0, long p6 = 0, long p7 = 0, long p8 = 0, long p9 = 0);
#endif

/// On OS X, use weak linking for wcsdup and wcscasecmp. Weak linking allows you to call the
/// function only if it exists at runtime. You can detect it by testing the function pointer against
/// NULL. To avoid making the callers do that, redefine wcsdup to wcsdup_use_weak, and likewise with
/// wcscasecmp. This lets us use the same binary on SnowLeopard (10.6) and Lion+ (10.7), even though
/// these functions only exist on 10.7+.
///
/// On other platforms, use what's detected at build time.
#if __APPLE__
#if __DARWIN_C_LEVEL >= 200809L
// We have to explicitly redeclare these as weak,
// since we are forced to set the MIN_REQUIRED availability macro to 10.7
// to use libc++, which in turn exposes these as strong
wchar_t *wcsdup(const wchar_t *) __attribute__((weak_import));
int wcscasecmp(const wchar_t *, const wchar_t *) __attribute__((weak_import));
int wcsncasecmp(const wchar_t *, const wchar_t *, size_t n) __attribute__((weak_import));
wchar_t *wcsdup_use_weak(const wchar_t *);
int wcscasecmp_use_weak(const wchar_t *, const wchar_t *);
int wcsncasecmp_use_weak(const wchar_t *s1, const wchar_t *s2, size_t n);
#define wcsdup(a) wcsdup_use_weak((a))
#define wcscasecmp(a, b) wcscasecmp_use_weak((a), (b))
#define wcsncasecmp(a, b, c) wcsncasecmp_use_weak((a), (b), (c))
#else
wchar_t *wcsdup(const wchar_t *in);
int wcscasecmp(const wchar_t *a, const wchar_t *b);
int wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wcsndup(const wchar_t *in, size_t c);
#endif
#else  //__APPLE__

/// These functions are missing from Solaris 10, and only accessible from
/// Solaris 11 in the std:: namespace.
#ifndef HAVE_WCSDUP
#ifdef HAVE_STD__WCSDUP
using std::wcsdup;
#else
wchar_t *wcsdup(const wchar_t *in);
#endif  // HAVE_STD__WCSDUP
#endif  // HAVE_WCSDUP

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
#endif

#ifndef HAVE_WCSNDUP
/// Fallback for wcsndup function. Returns a copy of \c in, truncated to a maximum length of \c c.
wchar_t *wcsndup(const wchar_t *in, size_t c);
#endif

#ifndef HAVE_WCSLCPY
/// Copy src to string dst of size siz.  At most siz-1 characters will be copied.  Always NUL
/// terminates (unless siz == 0).  Returns wcslen(src); if retval >= siz, truncation occurred.
///
/// This is the OpenBSD strlcpy function, modified for wide characters, and renamed to reflect this
/// change.
size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t siz);
#endif

#if 0
// These are not currently used.
#ifndef HAVE_LRAND48_R
/// Data structure for the lrand48_r fallback implementation.
struct drand48_data {
    unsigned int seed;
};

/// Fallback implementation of lrand48_r. Internally uses rand_r, so it is pretty weak.
int lrand48_r(struct drand48_data *buffer, long int *result);

/// Fallback implementation of srand48_r, the seed function for lrand48_r.
int srand48_r(long int seedval, struct drand48_data *buffer);
#endif
#endif

#ifndef HAVE_FUTIMES
int futimes(int fd, const struct timeval *times);
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

#endif

// NetBSD _has_ wcstod_l, but it's doing some weak linking hullabaloo that I don't get.
// Since it doesn't have uselocale (yes, the standard function isn't there, the non-standard extension is),
// we can't try to use the fallback.
#if !defined(HAVE_WCSTOD_L) && !defined(__NetBSD__)
// On some platforms if this is incorrectly detected and a system-defined
// defined version of `wcstod_l` exists, calling `wcstod` from our own
// `wcstod_l` can call back into `wcstod_l` causing infinite recursion.
// e.g. FreeBSD defines `wcstod(x, y)` as `wcstod_l(x, y, __get_locale())`.
// Solution: namespace our implementation to make sure there is no symbol
// duplication.
#undef wcstod_l
namespace fish_compat {
    double wcstod_l(const wchar_t *enptr, wchar_t **endptr, locale_t loc);
}
#define wcstod_l(x, y, z) fish_compat::wcstod_l(x, y, z)
#endif
