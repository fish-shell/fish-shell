//! A version of the getopt library for use with wide character strings.
//!
//! Note wgetopter expects an mutable array of const strings. It modifies the order of the
//! strings, but not their contents.
/* Declarations for getopt.
   Copyright (C) 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.

This file is part of the GNU C Library.  Its master source is NOT part of
the C library, however.  The master source lives in /gd/gnu/lib.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

use std::ops;

use crate::wchar::{wstr, WExt, WString, L};

/// Describe how to deal with options that follow non-option ARGV-elements.
///
/// If the caller did not specify anything, the default is PERMUTE.
///
/// REQUIRE_ORDER means don't recognize them as options; stop option processing when the first
/// non-option is seen. This is what Unix does. This mode of operation is selected by using `+'
/// as the first character of the list of option characters.
///
/// PERMUTE is the default.  We permute the contents of ARGV as we scan, so that eventually all
/// the non-options are at the end.  This allows options to be given in any order, even with
/// programs that were not written to expect this.
///
/// RETURN_IN_ORDER is an option available to programs that were written to expect options and
/// other ARGV-elements in any order and that care about the ordering of the two.  We describe
/// each non-option ARGV-element as if it were the argument of an option with character code 1.
/// Using `-` as the first character of the list of option characters selects this mode of
/// operation.
///
/// The special argument `--` forces an end of option-scanning regardless of the value of
/// `ordering`.  In the case of RETURN_IN_ORDER, only `--` can cause `getopt` to return EOF with
/// `woptind` != ARGC.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(clippy::upper_case_acronyms)]
enum Ordering {
    REQUIRE_ORDER,
    PERMUTE,
    RETURN_IN_ORDER,
}

impl Default for Ordering {
    fn default() -> Self {
        Ordering::PERMUTE
    }
}

fn empty_wstr() -> &'static wstr {
    Default::default()
}

pub struct wgetopter_t<'opts, 'argarray> {
    /// Argv.
    argv: &'argarray mut [WString],

    /// For communication from `getopt` to the caller. When `getopt` finds an option that takes an
    /// argument, the argument value is returned here. Also, when `ordering` is RETURN_IN_ORDER, each
    /// non-option ARGV-element is returned here.
    woptarg_idx: Option<(usize, usize)>,

    shortopts: &'opts wstr,
    longopts: &'opts [woption<'opts>],

    /// The next char to be scanned in the option-element in which the last option character we
    /// returned was found. This allows us to pick up the scan where we left off.
    ///
    /// If this is empty, it means resume the scan by advancing to the next ARGV-element.
    nextchar_slice: ops::Range<usize>,

    /// Index in ARGV of the next element to be scanned. This is used for communication to and from
    /// the caller and for communication between successive calls to `getopt`.
    ///
    /// On entry to `getopt`, zero means this is the first call; initialize.
    ///
    /// When `getopt` returns EOF, this is the index of the first of the non-option elements that the
    /// caller should itself scan.
    ///
    /// Otherwise, `woptind` communicates from one call to the next how much of ARGV has been scanned
    /// so far.
    // XXX 1003.2 says this must be 1 before any call.
    pub woptind: usize,

    /// Set to an option character which was unrecognized.
    woptopt: char,

    /// Describe how to deal with options that follow non-option ARGV-elements.
    ordering: Ordering,

    /// Handle permutation of arguments.
    ///
    /// Describe the part of ARGV that contains non-options that have been skipped.  `first_nonopt`
    /// is the index in ARGV of the first of them; `last_nonopt` is the index after the last of them.
    pub first_nonopt: usize,
    pub last_nonopt: usize,

    missing_arg_return_colon: bool,
    initialized: bool,
}

/// Names for the values of the `has_arg` field of `woption`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum woption_argument_t {
    no_argument,
    required_argument,
    optional_argument,
}

/// Describe the long-named options requested by the application. The LONG_OPTIONS argument to
/// getopt_long or getopt_long_only is a vector of `struct option' terminated by an element
/// containing a name which is zero.
///
/// The field `has_arg` is:
/// no_argument    (or 0) if the option does not take an argument,
/// required_argument  (or 1) if the option requires an argument,
/// optional_argument   (or 2) if the option takes an optional argument.
///
/// If the field `flag` is not NULL, it points to a variable that is set to the value given in the
/// field `val` when the option is found, but left unchanged if the option is not found.
///
/// To have a long-named option do something other than set an `int` to a compiled-in constant, such
/// as set a value from `optarg`, set the option's `flag` field to zero and its `val` field to a
/// nonzero value (the equivalent single-letter option character, if there is one).  For long
/// options that have a zero `flag` field, `getopt` returns the contents of the `val` field.
#[derive(Debug, Clone, Copy)]
pub struct woption<'a> {
    /// Long name for switch.
    pub name: &'a wstr,

    pub has_arg: woption_argument_t,

    /// If \c flag is non-null, this is the value that flag will be set to. Otherwise, this is the
    /// return-value of the function call.
    pub val: char,
}

/// Helper function to create a woption.
pub const fn wopt(name: &wstr, has_arg: woption_argument_t, val: char) -> woption<'_> {
    woption { name, has_arg, val }
}

impl<'opts, 'argarray> wgetopter_t<'opts, 'argarray> {
    pub fn new(
        shortopts: &'opts wstr,
        longopts: &'opts [woption],
        argv: &'argarray mut [WString],
    ) -> Self {
        return wgetopter_t {
            woptopt: '?',
            argv,
            shortopts,
            longopts,
            first_nonopt: 0,
            initialized: false,
            last_nonopt: 0,
            missing_arg_return_colon: false,
            nextchar_slice: 0..0,
            ordering: Ordering::PERMUTE,
            woptarg_idx: None,
            woptind: 0,
        };
    }

    pub fn wgetopt_long(&mut self) -> Option<char> {
        assert!(self.woptind <= self.argc(), "woptind is out of range");
        let mut ignored = 0;
        return self._wgetopt_internal(&mut ignored, false);
    }

    pub fn wgetopt_long_idx(&mut self, opt_index: &mut usize) -> Option<char> {
        return self._wgetopt_internal(opt_index, false);
    }

    pub fn woptarg(&self) -> Option<&wstr> {
        self.woptarg_idx
            .map(|(woptind, start)| &self.argv[woptind][start..])
    }

    pub fn cmd(&self) -> &wstr {
        &self.argv[0]
    }

    pub fn argv(&self) -> &[WString] {
        self.argv
    }

    fn nextchar(&self) -> &wstr {
        &self.argv[self.woptind][self.nextchar_slice.clone()]
    }

    /// \return the number of arguments.
    fn argc(&self) -> usize {
        return self.argv.len();
    }

    /// Exchange two adjacent subsequences of ARGV. One subsequence is elements
    /// [first_nonopt,last_nonopt) which contains all the non-options that have been skipped so far. The
    /// other is elements [last_nonopt,woptind), which contains all the options processed since those
    /// non-options were skipped.
    ///
    /// `first_nonopt` and `last_nonopt` are relocated so that they describe the new indices of the
    /// non-options in ARGV after they are moved.
    fn exchange(&mut self) {
        let mut bottom = self.first_nonopt;
        let middle = self.last_nonopt;
        let mut top = self.woptind;

        // Exchange the shorter segment with the far end of the longer segment. That puts the shorter
        // segment into the right place. It leaves the longer segment in the right place overall, but it
        // consists of two parts that need to be swapped next.
        while top > middle && middle > bottom {
            if top - middle > middle - bottom {
                // Bottom segment is the short one.
                let len = middle - bottom;

                // Swap it with the top part of the top segment.
                for i in 0..len {
                    self.argv.swap(bottom + i, top - (middle - bottom) + i);
                }
                // Exclude the moved bottom segment from further swapping.
                top -= len;
            } else {
                // Top segment is the short one.
                let len = top - middle;

                // Swap it with the bottom part of the bottom segment.
                for i in 0..len {
                    self.argv.swap(bottom + i, middle + i);
                }
                // Exclude the moved top segment from further swapping.
                bottom += len;
            }
        }

        // Update records for the slots the non-options now occupy.
        self.first_nonopt += self.woptind - self.last_nonopt;
        self.last_nonopt = self.woptind;
    }

    /// Initialize the internal data when the first call is made.
    fn _wgetopt_initialize(&mut self) {
        // Start processing options with ARGV-element 1 (since ARGV-element 0 is the program name); the
        // sequence of previously skipped non-option ARGV-elements is empty.
        self.first_nonopt = 1;
        self.last_nonopt = 1;
        self.woptind = 1;
        self.nextchar_slice = 0..0;

        let mut optstring = self.shortopts;

        // Determine how to handle the ordering of options and nonoptions.
        if optstring.char_at(0) == '-' {
            self.ordering = Ordering::RETURN_IN_ORDER;
            optstring = &optstring[1..];
        } else if optstring.char_at(0) == '+' {
            self.ordering = Ordering::REQUIRE_ORDER;
            optstring = &optstring[1..];
        } else {
            self.ordering = Ordering::PERMUTE;
        }

        if optstring.char_at(0) == ':' {
            self.missing_arg_return_colon = true;
            optstring = &optstring[1..];
        }

        self.shortopts = optstring;
        self.initialized = true;
    }

    /// Advance to the next ARGV-element.
    /// \return Some(\0) on success, or None or another value if we should stop.
    fn _advance_to_next_argv(&mut self) -> Option<char> {
        let argc = self.argc();
        if self.ordering == Ordering::PERMUTE {
            // If we have just processed some options following some non-options, exchange them so
            // that the options come first.
            if self.first_nonopt != self.last_nonopt && self.last_nonopt != self.woptind {
                self.exchange();
            } else if self.last_nonopt != self.woptind {
                self.first_nonopt = self.woptind;
            }

            // Skip any additional non-options and extend the range of non-options previously
            // skipped.
            while self.woptind < argc
                && (self.argv[self.woptind].char_at(0) != '-' || self.argv[self.woptind].len() == 1)
            {
                self.woptind += 1;
            }
            self.last_nonopt = self.woptind;
        }

        // The special ARGV-element `--' means premature end of options. Skip it like a null option,
        // then exchange with previous non-options as if it were an option, then skip everything
        // else like a non-option.
        if self.woptind != argc && self.argv[self.woptind] == "--" {
            self.woptind += 1;

            if self.first_nonopt != self.last_nonopt && self.last_nonopt != self.woptind {
                self.exchange();
            } else if self.first_nonopt == self.last_nonopt {
                self.first_nonopt = self.woptind;
            }
            self.last_nonopt = argc;
            self.woptind = argc;
        }

        // If we have done all the ARGV-elements, stop the scan and back over any non-options that
        // we skipped and permuted.

        if self.woptind == argc {
            // Set the next-arg-index to point at the non-options that we previously skipped, so the
            // caller will digest them.
            if self.first_nonopt != self.last_nonopt {
                self.woptind = self.first_nonopt;
            }
            return None;
        }

        // If we have come to a non-option and did not permute it, either stop the scan or describe
        // it to the caller and pass it by.
        if self.argv[self.woptind].char_at(0) != '-' || self.argv[self.woptind].len() == 1 {
            if self.ordering == Ordering::REQUIRE_ORDER {
                return None;
            }
            self.woptarg_idx = Some((self.woptind, 0));
            self.woptind += 1;
            return Some(char::from(1));
        }

        // We have found another option-ARGV-element. Skip the initial punctuation.
        let skip = if !self.longopts.is_empty() && self.argv[self.woptind].char_at(1) == '-' {
            2
        } else {
            1
        };
        self.nextchar_slice = skip..(self.argv[self.woptind].len());
        return Some(char::from(0));
    }

    /// Check for a matching short opt.
    fn _handle_short_opt(&mut self) -> char {
        // Look at and handle the next short option-character.
        let mut c = self.nextchar().char_at(0);
        self.nextchar_slice = self.nextchar_slice.start + 1..self.nextchar_slice.end;

        let temp = match self.shortopts.chars().position(|sc| sc == c) {
            Some(pos) => &self.shortopts[pos..],
            None => L!(""),
        };

        // Increment `woptind' when we start to process its last character.
        if self.nextchar().is_empty() {
            self.woptind += 1;
        }

        if temp.is_empty() || c == ':' {
            self.woptopt = c;

            if !self.nextchar().is_empty() {
                self.woptind += 1;
            }
            return '?';
        }

        if temp.char_at(1) != ':' {
            return c;
        }

        if temp.char_at(2) == ':' {
            // This is an option that accepts an argument optionally.
            if !self.nextchar().is_empty() {
                self.woptarg_idx = Some((self.woptind, 0));
                self.woptind += 1;
            } else {
                self.woptarg_idx = None;
            }
            self.nextchar_slice = 0..0;
        } else {
            // This is an option that requires an argument.
            if !self.nextchar().is_empty() {
                self.woptarg_idx = Some((self.woptind, 0));
                // If we end this ARGV-element by taking the rest as an arg, we must advance to
                // the next element now.
                self.woptind += 1;
            } else if self.woptind == self.argc() {
                self.woptopt = c;
                c = if self.missing_arg_return_colon {
                    ':'
                } else {
                    '?'
                };
            } else {
                // We already incremented `woptind' once; increment it again when taking next
                // ARGV-elt as argument.
                self.woptarg_idx = Some((self.woptind, 0));
                self.woptind += 1;
            }
            self.nextchar_slice = 0..0;
        }

        return c;
    }

    fn _update_long_opt(
        &mut self,
        pfound: &woption,
        nameend: usize,
        longind: &mut usize,
        option_index: usize,
        retval: &mut char,
    ) {
        self.woptind += 1;
        assert!(
            self.nextchar().char_at(nameend) == '\0' || self.nextchar().char_at(nameend) == '='
        );
        if self.nextchar().char_at(nameend) == '=' {
            if pfound.has_arg != woption_argument_t::no_argument {
                self.woptarg_idx = Some((self.woptind, nameend + 1));
            } else {
                self.nextchar_slice = 0..0;
                *retval = '?';
                return;
            }
        } else if pfound.has_arg == woption_argument_t::required_argument {
            if self.woptind < self.argc() {
                self.woptarg_idx = Some((self.woptind, 0));
                self.woptind += 1;
            } else {
                self.nextchar_slice = 0..0;
                *retval = if self.missing_arg_return_colon {
                    ':'
                } else {
                    '?'
                };
                return;
            }
        }

        self.nextchar_slice = 0..0;
        *longind = option_index;
        *retval = pfound.val;
    }

    /// Find a matching long opt.
    fn _find_matching_long_opt(
        &self,
        nameend: usize,
        exact: &mut bool,
        ambig: &mut bool,
        indfound: &mut usize,
    ) -> Option<woption<'opts>> {
        let mut pfound: Option<woption> = None;

        // Test all long options for either exact match or abbreviated matches.
        for (option_index, p) in self.longopts.iter().enumerate() {
            // Check if current option is prefix of long opt
            if p.name.starts_with(&self.nextchar()[..nameend]) {
                if nameend == p.name.len() {
                    // The current option is exact match of this long option
                    pfound = Some(*p);
                    *indfound = option_index;
                    *exact = true;
                    break;
                } else if pfound.is_none() {
                    // current option is first prefix match but not exact match
                    pfound = Some(*p);
                    *indfound = option_index;
                } else {
                    // current option is second or later prefix match but not exact match
                    *ambig = true;
                }
            }
        }
        return pfound;
    }

    /// Check for a matching long opt.
    fn _handle_long_opt(
        &mut self,
        longind: &mut usize,
        long_only: bool,
        retval: &mut char,
    ) -> bool {
        let mut exact = false;
        let mut ambig = false;
        let mut indfound: usize = 0;

        let mut nameend = 0;
        while self.nextchar().char_at(nameend) != '\0' && self.nextchar().char_at(nameend) != '=' {
            nameend += 1;
        }

        let pfound = self._find_matching_long_opt(nameend, &mut exact, &mut ambig, &mut indfound);

        if ambig && !exact {
            self.nextchar_slice = 0..0;
            self.woptind += 1;
            *retval = '?';
            return true;
        }

        if let Some(pfound) = pfound {
            self._update_long_opt(&pfound, nameend, longind, indfound, retval);
            return true;
        }

        // Can't find it as a long option.  If this is not getopt_long_only, or the option starts
        // with '--' or is not a valid short option, then it's an error. Otherwise interpret it as a
        // short option.
        if !long_only
            || self.argv[self.woptind].char_at(1) == '-'
            || !self
                .shortopts
                .as_char_slice()
                .contains(&self.nextchar().char_at(0))
        {
            self.nextchar_slice = 0..0;
            self.woptind += 1;
            *retval = '?';
            return true;
        }

        return false;
    }

    /// Scan elements of ARGV (whose length is ARGC) for option characters given in OPTSTRING.
    ///
    /// If an element of ARGV starts with '-', and is not exactly "-" or "--", then it is an option
    /// element.  The characters of this element (aside from the initial '-') are option characters.  If
    /// `getopt` is called repeatedly, it returns successively each of the option characters from each of
    /// the option elements.
    ///
    /// If `getopt` finds another option character, it returns that character, updating `woptind` and
    /// `nextchar` so that the next call to `getopt` can resume the scan with the following option
    /// character or ARGV-element.
    ///
    /// If there are no more option characters, `getopt` returns `EOF`. Then `woptind` is the index in
    /// ARGV of the first ARGV-element that is not an option.  (The ARGV-elements have been permuted so
    /// that those that are not options now come last.)
    ///
    /// OPTSTRING is a string containing the legitimate option characters. If an option character is seen
    /// that is not listed in OPTSTRING, return '?'.
    ///
    /// If a char in OPTSTRING is followed by a colon, that means it wants an arg, so the following text
    /// in the same ARGV-element, or the text of the following ARGV-element, is returned in `optarg`.
    /// Two colons mean an option that wants an optional arg; if there is text in the current
    /// ARGV-element, it is returned in `w.woptarg()`, otherwise `w.woptarg()` is set to zero.
    ///
    /// If OPTSTRING starts with `-` or `+', it requests different methods of handling the non-option
    /// ARGV-elements. See the comments about RETURN_IN_ORDER and REQUIRE_ORDER, above.
    ///
    /// Long-named options begin with `--` instead of `-`. Their names may be abbreviated as long as the
    /// abbreviation is unique or is an exact match for some defined option.  If they have an argument,
    /// it follows the option name in the same ARGV-element, separated from the option name by a `=', or
    /// else the in next ARGV-element. When `getopt` finds a long-named option, it returns 0 if that
    /// option's `flag` field is nonzero, the value of the option's `val` field if the `flag` field is
    /// zero.
    ///
    /// LONGOPTS is a vector of `struct option' terminated by an element containing a name which is zero.
    ///
    /// LONGIND returns the index in LONGOPT of the long-named option found. It is only valid when a
    /// long-named option has been found by the most recent call.
    ///
    /// If LONG_ONLY is nonzero, '-' as well as '--' can introduce long-named options.
    fn _wgetopt_internal(&mut self, longind: &mut usize, long_only: bool) -> Option<char> {
        if !self.initialized {
            self._wgetopt_initialize();
        }
        self.woptarg_idx = None;

        if self.nextchar().is_empty() {
            let narg = self._advance_to_next_argv();
            if narg != Some(char::from(0)) {
                return narg;
            }
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
        if !self.longopts.is_empty() && self.woptind < self.argc() {
            let arg = &self.argv[self.woptind];

            #[allow(clippy::if_same_then_else)]
            #[allow(clippy::needless_bool)]
            let try_long = if arg.char_at(0) == '-' && arg.char_at(1) == '-' {
                // Like --foo
                true
            } else if long_only && arg.len() >= 3 {
                // Like -fu
                true
            } else if !self.shortopts.as_char_slice().contains(&arg.char_at(1)) {
                // Like -f, but f is not a short arg.
                true
            } else {
                false
            };

            if try_long {
                let mut retval = '\0';
                if self._handle_long_opt(longind, long_only, &mut retval) {
                    return Some(retval);
                }
            }
        }

        return Some(self._handle_short_opt());
    }
}
