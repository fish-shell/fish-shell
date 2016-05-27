// A version of the getopt library for use with wide character strings.
//
// This is simply the gnu getopt library, but converted for use with wchar_t instead of char. This
// is not usually useful since the argv array is always defined to be of type char**, but in fish,
// all internal commands use wide characters and hence this library is useful.
//
// If you want to use this version of getopt in your program, download the fish sourcecode,
// available at <a href='http://fishshell.com'>the fish homepage</a>. Extract the sourcode, copy
// wgetopt.c and wgetopt.h into your program directory, include wgetopt.h in your program, and use
// all the regular getopt functions, prefixing every function, global variable and structure with a
// 'w', and use only wide character strings. There are no other functional changes in this version
// of getopt besides using wide character strings.
//
// For examples of how to use wgetopt, see the fish builtin functions, many of which are defined in
// builtin.c.

// Getopt for GNU.
//
// NOTE: getopt is now part of the C library, so if you don't know what "Keep this file name-space
// clean" means, talk to roland@gnu.ai.mit.edu before changing it!
//
// Copyright (C) 1987, 88, 89, 90, 91, 92, 93, 94
// Free Software Foundation, Inc.
//
// This file is part of the GNU C Library.  Its master source is NOT part of the C library, however.
// The master source lives in /gd/gnu/lib.
//
// The GNU C Library is free software; you can redistribute it and/or modify it under the terms of
// the GNU Library General Public License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// The GNU C Library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
// the GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License along with the GNU C
// Library; see the file COPYING.LIB.  If not, write to the Free Software Foundation, Inc., 675 Mass
// Ave, Cambridge, MA 02139, USA.
#include "config.h"

#include <stdio.h>
#include <wchar.h>
// This needs to come after some library #include to get __GNU_LIBRARY__ defined.
#ifdef __GNU_LIBRARY__
// Don't include stdlib.h for non-GNU C libraries because some of them contain conflicting
// prototypes for getopt.
#include <stdlib.h>
#endif  // GNU C library.

// This version of `getopt' appears to the caller like standard Unix `getopt' but it behaves
// differently for the user, since it allows the user to intersperse the options with the other
// arguments.
//
// As `getopt' works, it permutes the elements of ARGV so that, when it is done, all the options
// precede everything else.  Thus all application programs are extended to handle flexible argument
// order.
//
// GNU application programs can use a third alternative mode in which they can distinguish the
// relative order of options and other arguments.
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

// Use translation functions if available.
#ifdef _
#undef _
#endif

#ifdef HAVE_TRANSLATE_H
#ifdef USE_GETTEXT
#define _(string) wgettext(string)
#else
#define _(string) (string)
#endif
#else
#define _(wstr) wstr
#endif

#ifdef __GNU_LIBRARY__
// We want to avoid inclusion of string.h with non-GNU libraries because there are many ways it can
// cause trouble. On some systems, it contains special magic macros that don't work in GCC.
#include <string.h>  // IWYU pragma: keep
#define my_index wcschr
#else

// Avoid depending on library functions or files whose names are inconsistent.

char *getenv();

static wchar_t *my_index(const wchar_t *str, int chr) {
    while (*str) {
        if (*str == chr) return (wchar_t *)str;
        str++;
    }
    return 0;
}

// If using GCC, we can safely declare strlen this way. If not using GCC, it is ok not to declare
// it.
#ifdef __GNUC__
// Note that Motorola Delta 68k R3V7 comes with GCC but not stddef.h. That was relevant to code that
// was here before.
#if !defined(__STDC__) || !__STDC__
// gcc with -traditional declares the built-in strlen to return int, and has done so at least since
// version 2.4.5. -- rms.
extern int wcslen(const wchar_t *);
#endif  // not __STDC__
#endif  // __GNUC__

#endif  // not __GNU_LIBRARY__

// Exchange two adjacent subsequences of ARGV. One subsequence is elements
// [first_nonopt,last_nonopt) which contains all the non-options that have been skipped so far. The
// other is elements [last_nonopt,woptind), which contains all the options processed since those
// non-options were skipped.
//
// `first_nonopt' and `last_nonopt' are relocated so that they describe the new indices of the
// non-options in ARGV after they are moved.
void wgetopter_t::exchange(wchar_t **argv) {
    int bottom = first_nonopt;
    int middle = last_nonopt;
    int top = woptind;
    wchar_t *tem;

    // Exchange the shorter segment with the far end of the longer segment. That puts the shorter
    // segment into the right place. It leaves the longer segment in the right place overall, but it
    // consists of two parts that need to be swapped next.

    while (top > middle && middle > bottom) {
        if (top - middle > middle - bottom) {
            // Bottom segment is the short one.
            int len = middle - bottom;
            int i;

            // Swap it with the top part of the top segment.
            for (i = 0; i < len; i++) {
                tem = argv[bottom + i];
                argv[bottom + i] = argv[top - (middle - bottom) + i];
                argv[top - (middle - bottom) + i] = tem;
            }
            // Exclude the moved bottom segment from further swapping.
            top -= len;
        } else {
            // Top segment is the short one.
            int len = top - middle;
            int i;

            // Swap it with the bottom part of the bottom segment.
            for (i = 0; i < len; i++) {
                tem = argv[bottom + i];
                argv[bottom + i] = argv[middle + i];
                argv[middle + i] = tem;
            }
            // Exclude the moved top segment from further swapping.
            bottom += len;
        }
    }

    // Update records for the slots the non-options now occupy.
    first_nonopt += (woptind - last_nonopt);
    last_nonopt = woptind;
}

// Initialize the internal data when the first call is made.
const wchar_t *wgetopter_t::_wgetopt_initialize(const wchar_t *optstring) {
    // Start processing options with ARGV-element 1 (since ARGV-element 0 is the program name); the
    // sequence of previously skipped non-option ARGV-elements is empty.
    first_nonopt = last_nonopt = woptind = 1;
    nextchar = NULL;

    // Determine how to handle the ordering of options and nonoptions.
    if (optstring[0] == '-') {
        ordering = RETURN_IN_ORDER;
        ++optstring;
    } else if (optstring[0] == '+') {
        ordering = REQUIRE_ORDER;
        ++optstring;
    } else
        ordering = PERMUTE;

    return optstring;
}

// Scan elements of ARGV (whose length is ARGC) for option characters given in OPTSTRING.
//
// If an element of ARGV starts with '-', and is not exactly "-" or "--", then it is an option
// element.  The characters of this element (aside from the initial '-') are option characters.  If
// `getopt' is called repeatedly, it returns successively each of the option characters from each of
// the option elements.
//
// If `getopt' finds another option character, it returns that character, updating `woptind' and
// `nextchar' so that the next call to `getopt' can resume the scan with the following option
// character or ARGV-element.
//
// If there are no more option characters, `getopt' returns `EOF'. Then `woptind' is the index in
// ARGV of the first ARGV-element that is not an option.  (The ARGV-elements have been permuted so
// that those that are not options now come last.)
//
// OPTSTRING is a string containing the legitimate option characters. If an option character is seen
// that is not listed in OPTSTRING, return '?' after printing an error message.  If you set
// `wopterr' to zero, the error message is suppressed but we still return '?'.
//
// If a char in OPTSTRING is followed by a colon, that means it wants an arg, so the following text
// in the same ARGV-element, or the text of the following ARGV-element, is returned in `optarg'.
// Two colons mean an option that wants an optional arg; if there is text in the current
// ARGV-element, it is returned in `w.woptarg', otherwise `w.woptarg' is set to zero.
//
// If OPTSTRING starts with `-' or `+', it requests different methods of handling the non-option
// ARGV-elements. See the comments about RETURN_IN_ORDER and REQUIRE_ORDER, above.
//
// Long-named options begin with `--' instead of `-'. Their names may be abbreviated as long as the
// abbreviation is unique or is an exact match for some defined option.  If they have an argument,
// it follows the option name in the same ARGV-element, separated from the option name by a `=', or
// else the in next ARGV-element. When `getopt' finds a long-named option, it returns 0 if that
// option's `flag' field is nonzero, the value of the option's `val' field if the `flag' field is
// zero.
//
// LONGOPTS is a vector of `struct option' terminated by an element containing a name which is zero.
//
// LONGIND returns the index in LONGOPT of the long-named option found. It is only valid when a
// long-named option has been found by the most recent call.
//
// If LONG_ONLY is nonzero, '-' as well as '--' can introduce long-named options.
int wgetopter_t::_wgetopt_internal(int argc, wchar_t **argv, const wchar_t *optstring,
                                   const struct woption *longopts, int *longind, int long_only) {
    woptarg = NULL;

    if (woptind == 0) optstring = _wgetopt_initialize(optstring);

    if (nextchar == NULL || *nextchar == '\0') {
        // Advance to the next ARGV-element.
        if (ordering == PERMUTE) {
            // If we have just processed some options following some non-options, exchange them so
            // that the options come first.
            if (first_nonopt != last_nonopt && last_nonopt != woptind)
                exchange(argv);
            else if (last_nonopt != woptind)
                first_nonopt = woptind;

            // Skip any additional non-options and extend the range of non-options previously
            // skipped.
            while (woptind < argc && (argv[woptind][0] != '-' || argv[woptind][1] == '\0'))
                woptind++;
            last_nonopt = woptind;
        }

        // The special ARGV-element `--' means premature end of options. Skip it like a null option,
        // then exchange with previous non-options as if it were an option, then skip everything
        // else like a non-option.
        if (woptind != argc && !wcscmp(argv[woptind], L"--")) {
            woptind++;

            if (first_nonopt != last_nonopt && last_nonopt != woptind)
                exchange(argv);
            else if (first_nonopt == last_nonopt)
                first_nonopt = woptind;
            last_nonopt = argc;

            woptind = argc;
        }

        // If we have done all the ARGV-elements, stop the scan and back over any non-options that
        // we skipped and permuted.

        if (woptind == argc) {
            // Set the next-arg-index to point at the non-options that we previously skipped, so the
            // caller will digest them.
            if (first_nonopt != last_nonopt) woptind = first_nonopt;
            return EOF;
        }

        // If we have come to a non-option and did not permute it, either stop the scan or describe
        // it to the caller and pass it by.
        if ((argv[woptind][0] != '-' || argv[woptind][1] == '\0')) {
            if (ordering == REQUIRE_ORDER) return EOF;
            woptarg = argv[woptind++];
            return 1;
        }

        // We have found another option-ARGV-element. Skip the initial punctuation.
        nextchar = (argv[woptind] + 1 + (longopts != NULL && argv[woptind][1] == '-'));
    }

    // Decode the current option-ARGV-element.

    // Check whether the ARGV-element is a long option.
    //
    // If long_only and the ARGV-element has the form "-f", where f is a valid short option, don't
    // consider it an abbreviated form of a long option that starts with f.  Otherwise there would
    // be no way to give the -f short option.
    //
    // On the other hand, if there's a long option "fubar" and the ARGV-element is "-fu", do
    // consider that an abbreviation of the long option, just like "--fu", and not "-f" with arg
    // "u".
    //
    // This distinction seems to be the most useful approach.
    if (longopts != NULL &&
        (argv[woptind][1] == '-' ||
         (long_only && (argv[woptind][2] || !my_index(optstring, argv[woptind][1]))))) {
        wchar_t *nameend;
        const struct woption *p;
        const struct woption *pfound = NULL;
        int exact = 0;
        int ambig = 0;
        int indfound = 0;  // set to zero by Anton
        int option_index;

        for (nameend = nextchar; *nameend && *nameend != '='; nameend++) {
            // Do nothing.
        }

        // Test all long options for either exact match or abbreviated matches.
        for (p = longopts, option_index = 0; p->name; p++, option_index++)
            if (!wcsncmp(p->name, nextchar, nameend - nextchar)) {
                if ((unsigned int)(nameend - nextchar) == (unsigned int)wcslen(p->name)) {
                    // Exact match found.
                    pfound = p;
                    indfound = option_index;
                    exact = 1;
                    break;
                } else if (pfound == NULL) {
                    // First nonexact match found.
                    pfound = p;
                    indfound = option_index;
                } else
                    // Second or later nonexact match found.
                    ambig = 1;
            }

        if (ambig && !exact) {
            if (wopterr)
                fwprintf(stderr, _(L"%ls: Option '%ls' is ambiguous\n"), argv[0], argv[woptind]);
            nextchar += wcslen(nextchar);
            woptind++;
            return '?';
        }

        if (pfound != NULL) {
            option_index = indfound;
            woptind++;
            if (*nameend) {
                // Don't test has_arg with >, because some C compilers don't allow it to be used on
                // enums.
                if (pfound->has_arg)
                    woptarg = nameend + 1;
                else {
                    if (wopterr) {
                        if (argv[woptind - 1][1] == '-')  // --option
                            fwprintf(stderr, _(L"%ls: Option '--%ls' doesn't allow an argument\n"),
                                     argv[0], pfound->name);
                        else
                            // +option or -option
                            fwprintf(stderr, _(L"%ls: Option '%lc%ls' doesn't allow an argument\n"),
                                     argv[0], argv[woptind - 1][0], pfound->name);
                    }
                    nextchar += wcslen(nextchar);
                    return '?';
                }
            } else if (pfound->has_arg == 1) {
                if (woptind < argc)
                    woptarg = argv[woptind++];
                else {
                    if (wopterr)
                        fwprintf(stderr, _(L"%ls: Option '%ls' requires an argument\n"), argv[0],
                                 argv[woptind - 1]);
                    nextchar += wcslen(nextchar);
                    return optstring[0] == ':' ? ':' : '?';
                }
            }
            nextchar += wcslen(nextchar);
            if (longind != NULL) *longind = option_index;
            if (pfound->flag) {
                *(pfound->flag) = pfound->val;
                return 0;
            }
            return pfound->val;
        }

        // Can't find it as a long option.  If this is not getopt_long_only, or the option starts
        // with '--' or is not a valid short option, then it's an error. Otherwise interpret it as a
        // short option.
        if (!long_only || argv[woptind][1] == '-' || my_index(optstring, *nextchar) == NULL) {
            if (wopterr) {
                if (argv[woptind][1] == '-')  // --option
                    fwprintf(stderr, _(L"%ls: Unrecognized option '--%ls'\n"), argv[0], nextchar);
                else
                    // +option or -option
                    fwprintf(stderr, _(L"%ls: Unrecognized option '%lc%ls'\n"), argv[0],
                             argv[woptind][0], nextchar);
            }
            nextchar = (wchar_t *)L"";
            woptind++;
            return '?';
        }
    }

    // Look at and handle the next short option-character.
    {
        wchar_t c = *nextchar++;
        wchar_t *temp = const_cast<wchar_t *>(my_index(optstring, c));

        // Increment `woptind' when we start to process its last character.
        if (*nextchar == '\0') ++woptind;

        if (temp == NULL || c == ':') {
            if (wopterr) {
                fwprintf(stderr, _(L"%ls: Invalid option -- %lc\n"), argv[0], (wint_t)c);
            }
            woptopt = c;

            if (*nextchar != '\0') woptind++;

            return '?';
        }
        if (temp[1] == ':') {
            if (temp[2] == ':') {
                // This is an option that accepts an argument optionally.
                if (*nextchar != '\0') {
                    woptarg = nextchar;
                    woptind++;
                } else
                    woptarg = NULL;
                nextchar = NULL;
            } else {
                // This is an option that requires an argument.
                if (*nextchar != '\0') {
                    woptarg = nextchar;
                    // If we end this ARGV-element by taking the rest as an arg, we must advance to
                    // the next element now.
                    woptind++;
                } else if (woptind == argc) {
                    if (wopterr) {
                        // 1003.2 specifies the format of this message.
                        fwprintf(stderr, _(L"%ls: Option requires an argument -- %lc\n"), argv[0],
                                 (wint_t)c);
                    }
                    woptopt = c;
                    if (optstring[0] == ':')
                        c = ':';
                    else
                        c = '?';
                } else
                    // We already incremented `woptind' once; increment it again when taking next
                    // ARGV-elt as argument.
                    woptarg = argv[woptind++];
                nextchar = NULL;
            }
        }
        return c;
    }
}

int wgetopter_t::wgetopt_long(int argc, wchar_t **argv, const wchar_t *options,
                              const struct woption *long_options, int *opt_index) {
    return _wgetopt_internal(argc, argv, options, long_options, opt_index, 0);
}

int wgetopter_t::wgetopt_long_only(int argc, wchar_t **argv, const wchar_t *options,
                                   const struct woption *long_options, int *opt_index) {
    return _wgetopt_internal(argc, argv, options, long_options, opt_index, 1);
}
