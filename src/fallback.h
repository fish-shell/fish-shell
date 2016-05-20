#ifndef FISH_FALLBACK_H
#define FISH_FALLBACK_H

#include "config.h"

// IWYU likes to recommend adding <term.h> when we want <ncurses.h>. If we add <term.h> it breaks
// compiling several modules that include this header because they use symbols which are defined as
// macros in <term.h>.
// IWYU pragma: no_include <term.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
// The following include must be kept despite what IWYU says. That's because of the interaction
// between the weak linking of `wcsdup` and `wcscasecmp` via `#define`s below and the declarations
// in <wchar.h>. At least on OS X if we don't do this we get compilation errors do to the macro
// substitution if wchar.h is included after this header.
#include <wchar.h>  // IWYU pragma: keep
#if HAVE_NCURSES_H
#include <ncurses.h>  // IWYU pragma: keep
#endif

/// fish's internal versions of wcwidth and wcswidth, which can use an internal implementation if
/// the system one is busted.
int fish_wcwidth(wchar_t wc);
int fish_wcswidth(const wchar_t *str, size_t n);

#ifndef WCHAR_MAX
/// This _should_ be defined by wchar.h, but e.g. OpenBSD doesn't.
#define WCHAR_MAX INT_MAX
#endif

/// Under curses, tputs expects an int (*func)(char) as its last parameter, but in ncurses, tputs
/// expects a int (*func)(int) as its last parameter. tputs_arg_t is defined to always be what tputs
/// expects. Hopefully.
#ifdef NCURSES_VERSION
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

#ifdef TPARM_SOLARIS_KLUDGE
/// Solaris tparm has a set fixed of paramters in it's curses implementation, work around this here.
#define tparm tparm_solaris_kludge
char *tparm_solaris_kludge(char *str, ...);
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
#endif  //__APPLE__

#ifndef HAVE_WCSNDUP
/// Fallback for wcsndup function. Returns a copy of \c in, truncated to a maximum length of \c c.
wchar_t *wcsndup(const wchar_t *in, size_t c);
#endif

#ifndef HAVE_WCSLCAT
/// Appends src to string dst of size siz (unlike wcsncat, siz is the full size of dst, not space
/// left).  At most siz-1 characters will be copied.  Always NUL terminates (unless siz <=
/// wcslen(dst)).  Returns wcslen(src) + MIN(siz, wcslen(initial dst)).  If retval >= siz,
/// truncation occurred.
///
/// This is the OpenBSD strlcat function, modified for wide characters, and renamed to reflect this
/// change.
size_t wcslcat(wchar_t *dst, const wchar_t *src, size_t siz);
#endif
#ifndef HAVE_WCSLCPY
/// Copy src to string dst of size siz.  At most siz-1 characters will be copied.  Always NUL
/// terminates (unless siz == 0).  Returns wcslen(src); if retval >= siz, truncation occurred.
///
/// This is the OpenBSD strlcpy function, modified for wide characters, and renamed to reflect this
/// change.
size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t siz);
#endif

#ifndef HAVE_LRAND48_R
/// Datastructure for the lrand48_r fallback implementation.
struct drand48_data {
    unsigned int seed;
};

/// Fallback implementation of lrand48_r. Internally uses rand_r, so it is pretty weak.
int lrand48_r(struct drand48_data *buffer, long int *result);

/// Fallback implementation of srand48_r, the seed function for lrand48_r.
int srand48_r(long int seedval, struct drand48_data *buffer);
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

/// Cover for dcgettext.
char *fish_dcgettext(const char *domainname, const char *msgid, int category);

#ifndef HAVE__NL_MSG_CAT_CNTR
/// Some gettext implementation use have this variable, and by increasing it, one can tell the
/// system that the translations need to be reloaded.
extern int _nl_msg_cat_cntr;
#endif

#ifndef HAVE_KILLPG
/// Send specified signal to specified process group.
int killpg(int pgr, int sig);
#endif

#ifndef HAVE_NAN
double nan(char *tagp);
#endif

#endif
