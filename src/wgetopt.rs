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

use crate::wchar::prelude::*;

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
/// `wopt_index` != ARGC.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
enum Ordering {
    RequireOrder,
    #[default]
    Permute,
    ReturnInOrder,
}

/// The special character code, enabled via RETURN_IN_ORDER, indicating a non-option argument.
pub const NON_OPTION_CHAR: char = '\x01';

fn empty_wstr() -> &'static wstr {
    Default::default()
}

pub struct WGetopter<'opts, 'args, 'argarray> {
    /// Argv.
    pub argv: &'argarray mut [&'args wstr],

    /// For communication from `getopt` to the caller. When `getopt` finds an option that takes an
    /// argument, the argument value is returned here. Also, when `ordering` is RETURN_IN_ORDER, each
    /// non-option ARGV-element is returned here.
    pub woptarg: Option<&'args wstr>,

    shortopts: &'opts wstr,
    longopts: &'opts [WOption<'opts>],

    /// The next char to be scanned in the option-element in which the last option character we
    /// returned was found. This allows us to pick up the scan where we left off.
    ///
    /// If this is empty, it means resume the scan by advancing to the next ARGV-element.
    pub nextchar: &'args wstr,

    /// Index in ARGV of the next element to be scanned. This is used for communication to and from
    /// the caller and for communication between successive calls to `getopt`.
    ///
    /// On entry to `getopt`, zero means this is the first call; initialize.
    ///
    /// When `getopt` returns EOF, this is the index of the first of the non-option elements that the
    /// caller should itself scan.
    ///
    /// Otherwise, `wopt_index` communicates from one call to the next how much of ARGV has been scanned
    /// so far.
    // XXX 1003.2 says this must be 1 before any call.
    pub wopt_index: usize,

    /// Set to an option character which was unrecognized.
    unrecognized_opt: char,

    /// Describe how to deal with options that follow non-option ARGV-elements.
    ordering: Ordering,

    /// Handle permutation of arguments.
    ///
    /// Describe the part of ARGV that contains non-options that have been skipped.  `first_nonopt`
    /// is the index in ARGV of the first of them; `last_nonopt` is the index after the last of them.
    pub first_nonopt: usize,
    pub last_nonopt: usize,

    // Return `:` if an arg is missing.
    return_colon: bool,
    initialized: bool,
}

/// Names for the values of the `has_arg` field of `WOption`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ArgType {
    NoArgument,
    RequiredArgument,
    OptionalArgument,
}

/// Describe the long-named options requested by the application. The LONG_OPTIONS argument to
/// getopt_long or getopt_long_only is a vector of `struct option' terminated by an element
/// containing a name which is zero.
///
/// The field `has_arg` is:
/// NoArgument    (or 0) if the option does not take an argument,
/// RequiredArgument  (or 1) if the option requires an argument,
/// OptionalArgument   (or 2) if the option takes an optional argument.
///
/// If the field `flag` is not NULL, it points to a variable that is set to the value given in the
/// field `val` when the option is found, but left unchanged if the option is not found.
///
/// To have a long-named option do something other than set an `int` to a compiled-in constant, such
/// as set a value from `optarg`, set the option's `flag` field to zero and its `val` field to a
/// nonzero value (the equivalent single-letter option character, if there is one).  For long
/// options that have a zero `flag` field, `getopt` returns the contents of the `val` field.
#[derive(Debug, Clone, Copy)]
pub struct WOption<'a> {
    /// Long name for switch.
    pub name: &'a wstr,
    pub has_arg: ArgType,
    /// If \c flag is non-null, this is the value that flag will be set to. Otherwise, this is the
    /// return-value of the function call.
    pub val: char,
}

/// Helper function to create a WOption.
pub const fn wopt(name: &wstr, has_arg: ArgType, val: char) -> WOption<'_> {
    WOption { name, has_arg, val }
}

impl<'opts, 'args, 'argarray> WGetopter<'opts, 'args, 'argarray> {
    pub fn new(
        shortopts: &'opts wstr,
        longopts: &'opts [WOption],
        argv: &'argarray mut [&'args wstr],
    ) -> Self {
        return Self {
            unrecognized_opt: '?',
            argv,
            shortopts,
            longopts,
            first_nonopt: 0,
            initialized: false,
            last_nonopt: 0,
            return_colon: false,
            nextchar: Default::default(),
            ordering: Ordering::Permute,
            woptarg: None,
            wopt_index: 0,
        };
    }

    pub fn wgetopt_long(&mut self) -> Option<char> {
        assert!(
            self.wopt_index <= self.argv.len(),
            "wopt_index is out of range"
        );
        let mut ignored = 0;
        self.wgetopt_inner(&mut ignored)
    }

    pub fn wgetopt_long_idx(&mut self, opt_index: &mut usize) -> Option<char> {
        self.wgetopt_inner(opt_index)
    }

    /// Exchange two adjacent subsequences of ARGV. One subsequence is elements
    /// [first_nonopt,last_nonopt) which contains all the non-options that have been skipped so far. The
    /// other is elements [last_nonopt,wopt_index), which contains all the options processed since those
    /// non-options were skipped.
    ///
    /// `first_nonopt` and `last_nonopt` are relocated so that they describe the new indices of the
    /// non-options in ARGV after they are moved.
    fn exchange(&mut self) {
        let mut left = self.first_nonopt;
        let middle = self.last_nonopt;
        let mut right = self.wopt_index;

        // Exchange the shorter segment with the far end of the longer segment. That puts the shorter
        // segment into the right place. It leaves the longer segment in the right place overall, but it
        // consists of two parts that need to be swapped next.
        while right > middle && middle > left {
            if right - middle > middle - left {
                // Bottom segment is the short one.
                let len = middle - left;

                // Swap it with the top part of the top segment.
                for i in 0..len {
                    self.argv.swap(left + i, right - (middle - left) + i);
                }
                // Exclude the moved bottom segment from further swapping.
                right -= len;
            } else {
                // Top segment is the short one.
                let len = right - middle;

                // Swap it with the bottom part of the bottom segment.
                for i in 0..len {
                    self.argv.swap(left + i, middle + i);
                }
                // Exclude the moved top segment from further swapping.
                left += len;
            }
        }

        // Update records for the slots the non-options now occupy.
        self.first_nonopt += self.wopt_index - self.last_nonopt;
        self.last_nonopt = self.wopt_index;
    }

    /// Initialize the internal data when the first call is made.
    fn initialize(&mut self) {
        // Start processing options with ARGV-element 1 (since ARGV-element 0 is the program name); the
        // sequence of previously skipped non-option ARGV-elements is empty.
        self.first_nonopt = 1;
        self.last_nonopt = 1;
        self.wopt_index = 1;
        self.nextchar = empty_wstr();

        let mut optstring = self.shortopts;

        // Determine how to handle the ordering of options and nonoptions.
        if optstring.char_at(0) == '-' {
            self.ordering = Ordering::ReturnInOrder;
            optstring = &optstring[1..];
        } else if optstring.char_at(0) == '+' {
            self.ordering = Ordering::RequireOrder;
            optstring = &optstring[1..];
        } else {
            self.ordering = Ordering::Permute;
        }

        if optstring.char_at(0) == ':' {
            self.return_colon = true;
            optstring = &optstring[1..];
        }

        self.shortopts = optstring;
        self.initialized = true;
    }

    /// Advance to the next ARGV-element.
    /// \return Some(\0) on success, or None or another value if we should stop.
    fn next_argv(&mut self) -> Result<(), Option<char>> {
        let argc = self.argv.len();

        if self.ordering == Ordering::Permute {
            // If we have just processed some options following some non-options, exchange them so
            // that the options come first.
            if self.first_nonopt != self.last_nonopt && self.last_nonopt != self.wopt_index {
                self.exchange();
            } else if self.last_nonopt != self.wopt_index {
                self.first_nonopt = self.wopt_index;
            }

            // Skip any additional non-options and extend the range of non-options previously
            // skipped.
            while self.wopt_index < argc
                && (self.argv[self.wopt_index].char_at(0) != '-'
                    || self.argv[self.wopt_index].len() == 1)
            {
                self.wopt_index += 1;
            }
            self.last_nonopt = self.wopt_index;
        }

        // The special ARGV-element `--' means premature end of options. Skip it like a null option,
        // then exchange with previous non-options as if it were an option, then skip everything
        // else like a non-option.
        if self.wopt_index != argc && self.argv[self.wopt_index] == "--" {
            self.wopt_index += 1;

            if self.first_nonopt != self.last_nonopt && self.last_nonopt != self.wopt_index {
                self.exchange();
            } else if self.first_nonopt == self.last_nonopt {
                self.first_nonopt = self.wopt_index;
            }

            if self.first_nonopt == self.last_nonopt {
                self.first_nonopt = self.wopt_index;
            } else if self.last_nonopt != self.wopt_index {
                self.exchange();
            }

            self.last_nonopt = argc;
            self.wopt_index = argc;
        }

        // If we have done all the ARGV-elements, stop the scan and back over any non-options that
        // we skipped and permuted.

        if self.wopt_index == argc {
            // Set the next-arg-index to point at the non-options that we previously skipped, so the
            // caller will digest them.
            if self.first_nonopt != self.last_nonopt {
                self.wopt_index = self.first_nonopt;
            }
            return Err(None);
        }

        // If we have come to a non-option and did not permute it, either stop the scan or describe
        // it to the caller and pass it by.
        if self.argv[self.wopt_index].char_at(0) != '-' || self.argv[self.wopt_index].len() == 1 {
            if self.ordering == Ordering::RequireOrder {
                return Err(None);
            }
            self.woptarg = Some(self.argv[self.wopt_index]);
            self.wopt_index += 1;
            return Err(Some(NON_OPTION_CHAR));
        }

        // We have found another option-ARGV-element. Skip the initial punctuation.
        let skip = if !self.longopts.is_empty() && self.argv[self.wopt_index].char_at(1) == '-' {
            2
        } else {
            1
        };
        self.nextchar = self.argv[self.wopt_index][skip..].into();
        Ok(())
    }

    /// Check for a matching short opt.
    fn handle_short_opt(&mut self) -> char {
        // Look at and handle the next short option-character.
        let mut c = self.nextchar.char_at(0);
        self.nextchar = &self.nextchar[1..];

        let temp = match self.shortopts.chars().position(|sc| sc == c) {
            Some(pos) => &self.shortopts[pos..],
            None => L!(""),
        };

        // Increment `wopt_index' when we start to process its last character.
        if self.nextchar.is_empty() {
            self.wopt_index += 1;
        }

        if temp.is_empty() || c == ':' {
            self.unrecognized_opt = c;

            if !self.nextchar.is_empty() {
                self.wopt_index += 1;
            }
            return '?';
        }

        if temp.char_at(1) != ':' {
            return c;
        }

        if temp.char_at(2) == ':' {
            // This is an option that accepts an argument optionally.
            if !self.nextchar.is_empty() {
                self.woptarg = Some(self.nextchar);
                self.wopt_index += 1;
            } else {
                self.woptarg = None;
            }
            self.nextchar = empty_wstr();
        } else {
            // This is an option that requires an argument.
            if !self.nextchar.is_empty() {
                self.woptarg = Some(self.nextchar);
                // If we end this ARGV-element by taking the rest as an arg, we must advance to
                // the next element now.
                self.wopt_index += 1;
            } else if self.wopt_index == self.argv.len() {
                self.unrecognized_opt = c;
                c = if self.return_colon { ':' } else { '?' };
            } else {
                // We already incremented `wopt_index' once; increment it again when taking next
                // ARGV-elt as argument.
                self.woptarg = Some(self.argv[self.wopt_index]);
                self.wopt_index += 1;
            }
            self.nextchar = empty_wstr();
        }

        c
    }

    fn update_long_opt(
        &mut self,
        pfound: &WOption,
        nameend: usize,
        longind: &mut usize,
        option_index: usize,
    ) -> char {
        self.wopt_index += 1;
        assert!(self.nextchar.char_at(nameend) == '\0' || self.nextchar.char_at(nameend) == '=');
        if self.nextchar.char_at(nameend) == '=' {
            if pfound.has_arg != ArgType::NoArgument {
                self.woptarg = Some(self.nextchar[(nameend + 1)..].into());
            } else {
                self.nextchar = empty_wstr();
                return '?';
            }
        } else if pfound.has_arg == ArgType::RequiredArgument {
            if self.wopt_index < self.argv.len() {
                self.woptarg = Some(self.argv[self.wopt_index]);
                self.wopt_index += 1;
            } else {
                self.nextchar = empty_wstr();
                return if self.return_colon { ':' } else { '?' };
            }
        }

        self.nextchar = empty_wstr();
        *longind = option_index;
        pfound.val
    }

    /// Find a matching long opt.
    fn find_matching_long_opt(
        &self,
        nameend: usize,
        exact: &mut bool,
        ambig: &mut bool,
        indfound: &mut usize,
    ) -> Option<WOption<'opts>> {
        let mut pfound: Option<WOption> = None;

        // Test all long options for either exact match or abbreviated matches.
        for (option_index, p) in self.longopts.iter().enumerate() {
            // Check if current option is prefix of long opt
            if p.name.starts_with(&self.nextchar[..nameend]) {
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

        pfound
    }

    /// Check for a matching long opt.
    fn handle_long_opt(&mut self, longind: &mut usize) -> Option<char> {
        let mut exact = false;
        let mut ambig = false;
        let mut indfound: usize = 0;

        let mut nameend = 0;
        while self.nextchar.char_at(nameend) != '\0' && self.nextchar.char_at(nameend) != '=' {
            nameend += 1;
        }

        let pfound = self.find_matching_long_opt(nameend, &mut exact, &mut ambig, &mut indfound);

        if ambig && !exact {
            self.nextchar = empty_wstr();
            self.wopt_index += 1;
            return Some('?');
        }

        if let Some(pfound) = pfound {
            return Some(self.update_long_opt(&pfound, nameend, longind, indfound));
        }

        // Can't find it as a long option.  If this is not getopt_long_only, or the option starts
        // with '--' or is not a valid short option, then it's an error. Otherwise interpret it as a
        // short option.
        if self.argv[self.wopt_index].char_at(1) == '-'
            || !self
                .shortopts
                .as_char_slice()
                .contains(&self.nextchar.char_at(0))
        {
            self.nextchar = empty_wstr();
            self.wopt_index += 1;
            return Some('?');
        }

        None
    }

    /// Scan elements of ARGV (whose length is ARGC) for option characters given in OPTSTRING.
    ///
    /// If an element of ARGV starts with '-', and is not exactly "-" or "--", then it is an option
    /// element.  The characters of this element (aside from the initial '-') are option characters.  If
    /// `getopt` is called repeatedly, it returns successively each of the option characters from each of
    /// the option elements.
    ///
    /// If `getopt` finds another option character, it returns that character, updating `wopt_index` and
    /// `nextchar` so that the next call to `getopt` can resume the scan with the following option
    /// character or ARGV-element.
    ///
    /// If there are no more option characters, `getopt` returns `EOF`. Then `wopt_index` is the index in
    /// ARGV of the first ARGV-element that is not an option.  (The ARGV-elements have been permuted so
    /// that those that are not options now come last.)
    ///
    /// OPTSTRING is a string containing the legitimate option characters. If an option character is seen
    /// that is not listed in OPTSTRING, return '?'.
    ///
    /// If a char in OPTSTRING is followed by a colon, that means it wants an arg, so the following text
    /// in the same ARGV-element, or the text of the following ARGV-element, is returned in `optarg`.
    /// Two colons mean an option that wants an optional arg; if there is text in the current
    /// ARGV-element, it is returned in `w.woptarg`, otherwise `w.woptarg` is set to zero.
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
    fn wgetopt_inner(&mut self, longind: &mut usize) -> Option<char> {
        if !self.initialized {
            self.initialize();
        }
        self.woptarg = None;

        if self.nextchar.is_empty() {
            if let Err(narg) = self.next_argv() {
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
        if !self.longopts.is_empty() && self.wopt_index < self.argv.len() {
            let arg = self.argv[self.wopt_index];

            let try_long =
                // matches options like `--foo`
                arg.char_at(0) == '-' && arg.char_at(1) == '-'
                // matches options like `-f` if `f` is not a valid shortopt.
                || !self.shortopts.as_char_slice().contains(&arg.char_at(1));

            if try_long {
                if let Some(retval) = self.handle_long_opt(longind) {
                    return Some(retval);
                }
            }
        }

        Some(self.handle_short_opt())
    }
}
