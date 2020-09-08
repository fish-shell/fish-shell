// This file only contains fallback implementations of functions which have been found to be missing
// or broken by the configuration scripts.
//
// Many of these functions are more or less broken and incomplete.
#include "config.h"

// IWYU likes to recommend adding term.h when we want ncurses.h.
// IWYU pragma: no_include term.h
#include <dirent.h>  // IWYU pragma: keep
#include <errno.h>   // IWYU pragma: keep
#include <fcntl.h>   // IWYU pragma: keep
#include <limits.h>  // IWYU pragma: keep
#include <stdarg.h>  // IWYU pragma: keep
#include <stdio.h>   // IWYU pragma: keep
#include <stdlib.h>
#include <sys/stat.h>   // IWYU pragma: keep
#include <sys/types.h>  // IWYU pragma: keep
#include <unistd.h>
#include <wctype.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#if HAVE_GETTEXT
#include <libintl.h>
#endif
#if HAVE_CURSES_H
#include <curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>  // IWYU pragma: keep
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>  // IWYU pragma: keep
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <signal.h>  // IWYU pragma: keep

#include <cwchar>  // IWYU pragma: keep

#include "common.h"    // IWYU pragma: keep
#include "fallback.h"  // IWYU pragma: keep

#if defined(TPARM_SOLARIS_KLUDGE)
#undef tparm

char *tparm_solaris_kludge(char *str, long p1, long p2, long p3, long p4, long p5, long p6, long p7,
                           long p8, long p9) {
    return tparm(str, p1, p2, p3, p4, p5, p6, p7, p8, p9);
}

// Re-defining just to make sure nothing breaks further down in this file.
#define tparm tparm_solaris_kludge

#endif

int fish_mkstemp_cloexec(char *name_template) {
#if HAVE_MKOSTEMP
    // null check because mkostemp may be a weak symbol
    if (&mkostemp != nullptr) {
        return mkostemp(name_template, O_CLOEXEC);
    }
#endif
    int result_fd = mkstemp(name_template);
    if (result_fd != -1) {
        fcntl(result_fd, F_SETFD, FD_CLOEXEC);
    }
    return result_fd;
}

/// Fallback implementations of wcsdup and wcscasecmp. On systems where these are not needed (e.g.
/// building on Linux) these should end up just being stripped, as they are static functions that
/// are not referenced in this file.
// cppcheck-suppress unusedFunction
[[gnu::unused]] static wchar_t *wcsdup_fallback(const wchar_t *in) {
    size_t len = std::wcslen(in);
    auto out = static_cast<wchar_t *>(malloc(sizeof(wchar_t) * (len + 1)));
    if (out == nullptr) {
        return nullptr;
    }

    std::memcpy(out, in, sizeof(wchar_t) * (len + 1));
    return out;
}

[[gnu::unused]] static int wcscasecmp_fallback(const wchar_t *a, const wchar_t *b) {
    if (*a == 0) {
        return *b == 0 ? 0 : -1;
    } else if (*b == 0) {
        return 1;
    }
    int diff = towlower(*a) - towlower(*b);
    if (diff != 0) {
        return diff;
    }
    return wcscasecmp_fallback(a + 1, b + 1);
}

[[gnu::unused]] static int wcsncasecmp_fallback(const wchar_t *a, const wchar_t *b, size_t count) {
    if (count == 0) return 0;

    if (*a == 0) {
        return *b == 0 ? 0 : -1;
    } else if (*b == 0) {
        return 1;
    }
    int diff = towlower(*a) - towlower(*b);
    if (diff != 0) return diff;
    return wcsncasecmp_fallback(a + 1, b + 1, count - 1);
}

#ifndef HAVE_WCSDUP
#ifndef HAVE_STD__WCSDUP
wchar_t *wcsdup(const wchar_t *in) { return wcsdup_fallback(in); }
#endif
#endif

#ifndef HAVE_WCSCASECMP
#ifndef HAVE_STD__WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b) { return wcscasecmp_fallback(a, b); }
#endif
#endif

#ifndef HAVE_WCSNCASECMP
#ifndef HAVE_STD__WCSNCASECMP
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t n) {
    return wcsncasecmp_fallback(a, b, n);
}
#endif
#endif

#ifndef HAVE_WCSNDUP
wchar_t *wcsndup(const wchar_t *in, size_t c) {
    auto res = static_cast<wchar_t *>(malloc(sizeof(wchar_t) * (c + 1)));
    if (res == nullptr) {
        return nullptr;
    }
    wcslcpy(res, in, c + 1);
    return res;
}
#endif

#ifndef HAVE_WCSLCPY
/*$OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t siz) {
    wchar_t *d = dst;
    const wchar_t *s = src;
    size_t n = siz;

    // Copy as many bytes as will fit.
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0) break;
        } while (--n != 0);
    }

    // Not enough room in dst, add NUL and traverse rest of src.
    if (n == 0) {
        if (siz != 0) *d = '\0';  // NUL-terminate dst
        while (*s++)
            ;  // ignore rest of src
    }
    return s - src - 1;  // count does not include NUL
}
#endif

#ifndef HAVE_FUTIMES
int futimes(int fd, const struct timeval *times) {
    errno = ENOSYS;
    return -1;
}
#endif

#if HAVE_GETTEXT
char *fish_gettext(const char *msgid) { return gettext(msgid); }

char *fish_bindtextdomain(const char *domainname, const char *dirname) {
    return bindtextdomain(domainname, dirname);
}

char *fish_textdomain(const char *domainname) { return textdomain(domainname); }
#else
char *fish_gettext(const char *msgid) { return (char *)msgid; }
char *fish_bindtextdomain(const char *domainname, const char *dirname) {
    UNUSED(domainname);
    UNUSED(dirname);
    return NULL;
}
char *fish_textdomain(const char *domainname) {
    UNUSED(domainname);
    return NULL;
}
#endif

#ifndef HAVE_KILLPG
int killpg(int pgr, int sig) {
    assert(pgr > 1);
    return kill(-pgr, sig);
}
#endif

// Width of ambiguous characters. 1 is typical default.
int g_fish_ambiguous_width = 1;

// Width of emoji characters.
int g_fish_emoji_width = 0;

// 1 is the typical emoji width in Unicode 8.
int g_guessed_fish_emoji_width = 1;

static int fish_get_emoji_width(wchar_t c) {
    (void)c;
    // Respect an explicit value. If we don't have one, use the guessed value. Do not try to fall
    // back to wcwidth(), it's hopeless.
    if (g_fish_emoji_width > 0) return g_fish_emoji_width;
    return g_guessed_fish_emoji_width;
}

// Big hack to use our versions of wcswidth where we know them to be broken, which is
// EVERYWHERE (https://github.com/fish-shell/fish-shell/issues/2199)
#include "widecharwidth/widechar_width.h"

int fish_wcwidth(wchar_t wc) {
    // The system version of wcwidth should accurately reflect the ability to represent characters
    // in the console session, but knows nothing about the capabilities of other terminal emulators
    // or ttys. Use it from the start only if we are logged in to the physical console.
    if (is_console_session()) {
        return wcwidth(wc);
    }

    // Check for VS16 which selects emoji presentation. This "promotes" a character like U+2764
    // (width 1) to an emoji (probably width 2). So treat it as width 1 so the sums work. See #2652.
    // VS15 selects text presentation.
    const wchar_t variation_selector_16 = L'\uFE0F', variation_selector_15 = L'\uFE0E';
    if (wc == variation_selector_16)
        return 1;
    else if (wc == variation_selector_15)
        return 0;

    // Korean Hangul Jamo median vowels and final consonants.
    // These can either appear in combined form, taking 0 width themselves,
    // or standalone with a 1 width. Since that's literally not expressible with wcwidth(),
    // we take the position that the typical way for them to show up is composed.
    if (wc >= L'\u1160' && wc <= L'\u11FF') return 0;
    int width = widechar_wcwidth(wc);

    switch (width) {
        case widechar_nonprint:
        case widechar_combining:
        case widechar_unassigned:
            // Fall back to system wcwidth in this case.
            return wcwidth(wc);
        case widechar_ambiguous:
            return g_fish_ambiguous_width;
        case widechar_private_use:
            // TR11: "All private-use characters are by default classified as Ambiguous".
            return g_fish_ambiguous_width;
        case widechar_widened_in_9:
            return fish_get_emoji_width(wc);
        default:
            assert(width > 0 && "Unexpectedly nonpositive width");
            return width;
    }
}

int fish_wcswidth(const wchar_t *str, size_t n) {
    int result = 0;
    for (size_t i = 0; i < n && str[i] != L'\0'; i++) {
        int w = fish_wcwidth(str[i]);
        if (w < 0) {
            result = -1;
            break;
        }
        result += w;
    }
    return result;
}

#ifndef HAVE_FLOCK
/*	$NetBSD: flock.c,v 1.6 2008/04/28 20:24:12 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Todd Vierling.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Emulate flock() with fcntl().
 */

int flock(int fd, int op) {
    int rc = 0;

    struct flock fl = {0};

    switch (op & (LOCK_EX | LOCK_SH | LOCK_UN)) {
        case LOCK_EX:
            fl.l_type = F_WRLCK;
            break;

        case LOCK_SH:
            fl.l_type = F_RDLCK;
            break;

        case LOCK_UN:
            fl.l_type = F_UNLCK;
            break;

        default:
            errno = EINVAL;
            return -1;
    }

    fl.l_whence = SEEK_SET;
    rc = fcntl(fd, op & LOCK_NB ? F_SETLK : F_SETLKW, &fl);

    if (rc && (errno == EAGAIN)) errno = EWOULDBLOCK;

    return rc;
}

#endif  // HAVE_FLOCK

#if !defined(HAVE_WCSTOD_L) && !defined(__NetBSD__)
#undef wcstod_l
#include <locale.h>
// For platforms without wcstod_l C extension, wrap wcstod after changing the
// thread-specific locale.
double fish_compat::wcstod_l(const wchar_t *enptr, wchar_t **endptr, locale_t loc) {
    locale_t prev_locale = uselocale(loc);
    double ret = std::wcstod(enptr, endptr);
    uselocale(prev_locale);
    return ret;
}
#endif  // defined(wcstod_l)
