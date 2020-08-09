// A version of the getopt library for use with wide character strings.
//
// This is simply the gnu getopt library, but converted for use with wchar_t instead of char. This
// is not usually useful since the argv array is always defined to be of type char**, but in fish,
// all internal commands use wide characters and hence this library is useful.
//
// If you want to use this version of getopt in your program, download the fish sourcecode,
// available at <a href='https://fishshell.com'>the fish homepage</a>. Extract the sourcode, copy
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
#include "config.h"  // IWYU pragma: keep

#include <stdio.h>

#include <cstring>
#include <cwchar>

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
void wgetopter_t::_wgetopt_initialize(const wchar_t *optstring) {
    // Start processing options with ARGV-element 1 (since ARGV-element 0 is the program name); the
    // sequence of previously skipped non-option ARGV-elements is empty.
    first_nonopt = last_nonopt = woptind = 1;
    nextchar = nullptr;

    // Determine how to handle the ordering of options and nonoptions.
    if (optstring[0] == '-') {
        ordering = RETURN_IN_ORDER;
        ++optstring;
    } else if (optstring[0] == '+') {
        ordering = REQUIRE_ORDER;
        ++optstring;
    } else {
        ordering = PERMUTE;
    }

    if (optstring[0] == ':') {
        missing_arg_return_colon = true;
        ++optstring;
    }

    shortopts = optstring;
    initialized = true;
}

// Advance to the next ARGV-element.
int wgetopter_t::_advance_to_next_argv(  //!OCLINT(high cyclomatic complexity)
    int argc, wchar_t **argv, const struct woption *longopts) {
    if (ordering == PERMUTE) {
        // If we have just processed some options following some non-options, exchange them so
        // that the options come first.
        if (first_nonopt != last_nonopt && last_nonopt != woptind) {
            exchange(argv);
        } else if (last_nonopt != woptind) {
            first_nonopt = woptind;
        }

        // Skip any additional non-options and extend the range of non-options previously
        // skipped.
        while (woptind < argc && (argv[woptind][0] != '-' || argv[woptind][1] == '\0')) {
            woptind++;
        }
        last_nonopt = woptind;
    }

    // The special ARGV-element `--' means premature end of options. Skip it like a null option,
    // then exchange with previous non-options as if it were an option, then skip everything
    // else like a non-option.
    if (woptind != argc && !std::wcscmp(argv[woptind], L"--")) {
        woptind++;

        if (first_nonopt != last_nonopt && last_nonopt != woptind) {
            exchange(argv);
        } else if (first_nonopt == last_nonopt) {
            first_nonopt = woptind;
        }
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
    nextchar = (argv[woptind] + 1 + (longopts != nullptr && argv[woptind][1] == '-'));
    return 0;
}

// Check for a matching short opt.
int wgetopter_t::_handle_short_opt(int argc, wchar_t **argv) {
    // Look at and handle the next short option-character.
    wchar_t c = *nextchar++;
    const wchar_t *temp = std::wcschr(shortopts, c);

    // Increment `woptind' when we start to process its last character.
    if (*nextchar == '\0') ++woptind;

    if (temp == nullptr || c == ':') {
        if (wopterr) {
            std::fwprintf(stderr, _(L"%ls: Invalid option -- %lc\n"), argv[0],
                          static_cast<wint_t>(c));
        }
        woptopt = c;

        if (*nextchar != '\0') woptind++;
        return '?';
    }

    if (temp[1] != ':') {
        return c;
    }

    if (temp[2] == ':') {
        // This is an option that accepts an argument optionally.
        if (*nextchar != '\0') {
            woptarg = nextchar;
            woptind++;
        } else {
            woptarg = nullptr;
        }
        nextchar = nullptr;
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
                std::fwprintf(stderr, _(L"%ls: Option requires an argument -- %lc\n"), argv[0],
                              static_cast<wint_t>(c));
            }
            woptopt = c;
            c = missing_arg_return_colon ? ':' : '?';
        } else {
            // We already incremented `woptind' once; increment it again when taking next
            // ARGV-elt as argument.
            woptarg = argv[woptind++];
        }
        nextchar = nullptr;
    }

    return c;
}

void wgetopter_t::_update_long_opt(int argc, wchar_t **argv, const struct woption *pfound,
                                   wchar_t *nameend, int *longind, int option_index, int *retval) {
    woptind++;
    if (*nameend) {
        // Don't test has_arg with >, because some C compilers don't allow it to be used on
        // enums.
        if (pfound->has_arg)
            woptarg = nameend + 1;
        else {
            if (wopterr) {
                if (argv[woptind - 1][1] == '-')  // --option
                    std::fwprintf(stderr, _(L"%ls: Option '--%ls' doesn't allow an argument\n"),
                                  argv[0], pfound->name);
                else
                    // +option or -option
                    std::fwprintf(stderr, _(L"%ls: Option '%lc%ls' doesn't allow an argument\n"),
                                  argv[0], argv[woptind - 1][0], pfound->name);
            }
            nextchar += std::wcslen(nextchar);
            *retval = '?';
            return;
        }
    } else if (pfound->has_arg == 1) {
        if (woptind < argc)
            woptarg = argv[woptind++];
        else {
            if (wopterr)
                std::fwprintf(stderr, _(L"%ls: Option '%ls' requires an argument\n"), argv[0],
                              argv[woptind - 1]);
            nextchar += std::wcslen(nextchar);
            *retval = missing_arg_return_colon ? ':' : '?';
            return;
        }
    }

    nextchar += std::wcslen(nextchar);
    if (longind != nullptr) *longind = option_index;
    if (pfound->flag) {
        *(pfound->flag) = pfound->val;
        *retval = 0;
    } else {
        *retval = pfound->val;
    }
}

// Find a matching long opt.
const struct woption *wgetopter_t::_find_matching_long_opt(const struct woption *longopts,
                                                           wchar_t *nameend, int *exact, int *ambig,
                                                           int *indfound) const {
    const struct woption *pfound = nullptr;
    int option_index = 0;

    // Test all long options for either exact match or abbreviated matches.
    for (const struct woption *p = longopts; p->name; p++, option_index++) {
        if (!std::wcsncmp(p->name, nextchar, nameend - nextchar)) {
            if (static_cast<unsigned int>(nameend - nextchar) ==
                static_cast<unsigned int>(wcslen(p->name))) {
                // Exact match found.
                pfound = p;
                *indfound = option_index;
                *exact = 1;
                break;
            } else if (pfound == nullptr) {
                // First nonexact match found.
                pfound = p;
                *indfound = option_index;
            } else
                // Second or later nonexact match found.
                *ambig = 1;
        }
    }

    return pfound;
}

// Check for a matching long opt.
bool wgetopter_t::_handle_long_opt(int argc, wchar_t **argv, const struct woption *longopts,
                                   int *longind, int long_only, int *retval) {
    int exact = 0;
    int ambig = 0;
    int indfound = 0;

    wchar_t *nameend;
    for (nameend = nextchar; *nameend && *nameend != '='; nameend++)
        ;  //!OCLINT(empty body)

    const struct woption *pfound =
        _find_matching_long_opt(longopts, nameend, &exact, &ambig, &indfound);

    if (ambig && !exact) {
        if (wopterr) {
            std::fwprintf(stderr, _(L"%ls: Option '%ls' is ambiguous\n"), argv[0], argv[woptind]);
        }
        nextchar += std::wcslen(nextchar);
        woptind++;
        *retval = '?';
        return true;
    }

    if (pfound) {
        _update_long_opt(argc, argv, pfound, nameend, longind, indfound, retval);
        return true;
    }

    // Can't find it as a long option.  If this is not getopt_long_only, or the option starts
    // with '--' or is not a valid short option, then it's an error. Otherwise interpret it as a
    // short option.
    if (!long_only || argv[woptind][1] == '-' || std::wcschr(shortopts, *nextchar) == nullptr) {
        if (wopterr) {
            if (argv[woptind][1] == '-')  // --option
                std::fwprintf(stderr, _(L"%ls: Unrecognized option '--%ls'\n"), argv[0], nextchar);
            else
                // +option or -option
                std::fwprintf(stderr, _(L"%ls: Unrecognized option '%lc%ls'\n"), argv[0],
                              argv[woptind][0], nextchar);
        }
        nextchar = const_cast<wchar_t *>(L"");
        woptind++;
        *retval = '?';
        return true;
    }

    return false;
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
    if (!initialized) _wgetopt_initialize(optstring);
    woptarg = nullptr;

    if (nextchar == nullptr || *nextchar == '\0') {
        int retval = _advance_to_next_argv(argc, argv, longopts);
        if (retval != 0) return retval;
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
    if (longopts != nullptr &&
        (argv[woptind][1] == '-' ||
         (long_only && (argv[woptind][2] || !std::wcschr(shortopts, argv[woptind][1]))))) {
        int retval;
        if (_handle_long_opt(argc, argv, longopts, longind, long_only, &retval)) return retval;
    }

    return _handle_short_opt(argc, argv);
}

int wgetopter_t::wgetopt_long(int argc, wchar_t **argv, const wchar_t *options,
                              const struct woption *long_options, int *opt_index) {
    return _wgetopt_internal(argc, argv, options, long_options, opt_index, 0);
}
