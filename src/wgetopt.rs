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

/// Describes how to deal with options that follow non-option elements in `argv`.
///
/// Note that any arguments passed after `--` will be treated as non-option elements,
/// regardless of [Ordering].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
enum Ordering {
    /// Stop processing options when the first non-option element is encountered.
    /// Traditionally used by Unix systems.
    ///
    /// Indicated by using `+` as the first character in the optstring.
    RequireOrder,
    /// The eleemnts in `argv` are reordered so that non-option arguments end up
    /// at the end. This allows options to be given in any order, even with programs
    /// that were not written to expect this.
    #[default]
    Permute,
    /// Return both options and non-options in the order. Non-option arguments are
    /// treated as if they were arguments to an option identified by [NON_OPTION_CHAR].
    ///
    /// Indicated by using `-` as the first character in the optstring.
    ReturnInOrder,
}

/// Special char used with [Ordering::ReturnInOrder].
pub const NON_OPTION_CHAR: char = '\x01';

/// Utility function to quickly return a reference to an empty wstr.
fn empty_wstr() -> &'static wstr {
    Default::default()
}

pub struct WGetopter<'opts, 'args, 'argarray> {
    /// List of arguments. Will not be resized, but can be modified.
    pub argv: &'argarray mut [&'args wstr],
    /// Stores the arg of an argument-taking option, including the pseudo-arguments
    /// used by [Ordering::ReturnInOrder].
    pub woptarg: Option<&'args wstr>,
    /// Stores the optstring for short-named options.
    shortopts: &'opts wstr,
    /// Stores the data for long options.
    longopts: &'opts [WOption<'opts>],
    /// The remaining text of the current element, recorded so that we can pick up the
    /// scan from where we left off.
    pub remaining_text: &'args wstr,
    /// Index of the next element in `argv` to be scanned. If the value is `0`, then
    /// the next call will initialize. When scanning is finished, this marks the index
    /// of the first non-option element that should be parsed by the caller.
    // XXX 1003.2 says this must be 1 before any call.
    pub wopt_index: usize,
    /// Set when a (short) option is unrecognized.
    unrecognized_opt: char,
    /// How to deal with non-option elements following options.
    ordering: Ordering,
    /// Used when reordering elements. After scanning is finished, indicates the index
    /// of the first non-option skipped during parsing.
    pub first_nonopt: usize,
    /// Used when reordering elements. After scanning is finished, indicates the index
    /// after the final non-option skipped during parsing.
    pub last_nonopt: usize,
    /// Return `:` if an arg is missing.
    return_colon: bool,
    /// Prevents redundant initialization.
    initialized: bool,
}

/// Indicates whether a long-named takes an argument, and whether that argument
/// is optional.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ArgType {
    /// The option takes no arguments.
    NoArgument,
    /// The option takes a required argument.
    RequiredArgument,
    /// The option takes an optional argument.
    OptionalArgument,
}

/// Used to describe the properties of a long-named option.
#[derive(Debug, Clone, Copy)]
pub struct WOption<'a> {
    /// The long name of the option.
    pub name: &'a wstr,
    /// Whether the option takes an argument.
    pub has_arg: ArgType,
    /// Contains the return value of the function call.
    // unsure as to what this does in this version of the code
    pub val: char,
}

/// Helper function to create a WOption.
pub const fn wopt(name: &wstr, has_arg: ArgType, val: char) -> WOption<'_> {
    WOption { name, has_arg, val }
}

/// Used internally by [Wgetopter::find_matching_long_opt]. See there for further
/// details.
#[derive(Default)]
enum LongOptMatch<'a> {
    Exact(WOption<'a>, usize),
    NonExact(WOption<'a>, usize),
    Ambiguous,
    #[default]
    NoMatch,
}

impl<'opts, 'args, 'argarray> WGetopter<'opts, 'args, 'argarray> {
    pub fn new(
        shortopts: &'opts wstr,
        longopts: &'opts [WOption],
        argv: &'argarray mut [&'args wstr],
    ) -> Self {
        Self {
            argv,
            woptarg: None,
            shortopts,
            longopts,
            remaining_text: Default::default(),
            wopt_index: 0,
            unrecognized_opt: '?',
            ordering: Ordering::Permute,
            first_nonopt: 0,
            last_nonopt: 0,
            return_colon: false,
            initialized: false,
        }
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

    /// Divides `argv` into two lists, the one which contains all non-options skipped
    /// during scanning, and the other which contains all options scanned afterwards.
    ///
    /// Elements are then swapped between the list so that all options end up
    /// in the former list, and non-options in the latter.
    fn exchange(&mut self) {
        let mut left = self.first_nonopt;
        let middle = self.last_nonopt;
        let mut right = self.wopt_index;

        // If the two lists are equal in length, we swap them directly.
        // Otherwise we do it manually.
        if right - middle + 1 == middle - left {
            // ... I *think* this implementation makes sense?
            let (front, mut back) = self.argv.get_mut(left..right).unwrap().split_at_mut(middle);
            front.swap_with_slice(&mut back);
        } else {
            while right > middle && middle > left {
                if right - middle > middle - left {
                    // The left segment is the short one.
                    let len = middle - left;

                    // Swap it with the top part of the right segment.
                    for i in 0..len {
                        self.argv.swap(left + i, right - len + i);
                    }

                    // Exclude the moved elements from further swapping.
                    right -= len;
                } else {
                    // The right segment is the short one.
                    let len = right - middle;

                    // Swap it with the bottom part of the left segment.
                    for i in 0..len {
                        self.argv.swap(left + i, middle + i);
                    }

                    // Exclude the moved elements from further swapping.
                    left += len;
                }
            }
        }

        // Update the indices to indicate the new positions of the non-option
        // arguments.
        self.first_nonopt += self.wopt_index - self.last_nonopt;
        self.last_nonopt = self.wopt_index;
    }

    /// Initialize the internal data when the first call is made.
    fn initialize(&mut self) {
        // Skip the first element since it's just the program name.
        self.first_nonopt = 1;
        self.last_nonopt = 1;
        self.wopt_index = 1;
        self.remaining_text = empty_wstr();

        let mut optstring = self.shortopts;

        // Set ordering and strip markers.
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

    /// Advance to the next element in `argv`. Returns nothing on success, or
    /// an optional value if we need to stop.
    fn next_argv(&mut self) -> Result<(), Option<char>> {
        let argc = self.argv.len();

        if self.ordering == Ordering::Permute {
            // Permute the args if we've found options following non-options.
            if self.last_nonopt != self.wopt_index {
                if self.first_nonopt != self.last_nonopt {
                    self.exchange();
                } else {
                    self.first_nonopt = self.wopt_index;
                }
            }

            // Skip any further non-options, adjusting the relevant indices as
            // needed.
            while self.wopt_index < argc
                && (self.argv[self.wopt_index].char_at(0) != '-'
                    || self.argv[self.wopt_index].len() == 1)
            {
                self.wopt_index += 1;
            }

            self.last_nonopt = self.wopt_index;
        }

        // The `--` element prevents any further scanning of options. We skip it like
        // a null option, then swap with non-options as if were an option, then skip
        // everything else like a non-option.
        if self.wopt_index != argc && self.argv[self.wopt_index] == "--" {
            self.wopt_index += 1;

            if self.first_nonopt == self.last_nonopt {
                self.first_nonopt = self.wopt_index;
            } else if self.last_nonopt != self.wopt_index {
                self.exchange();
            }

            self.last_nonopt = argc;
            self.wopt_index = argc;
        }

        // If we're done with all the elements, stop scanning and back over any
        // non-options that were skipped and permuted.
        if self.wopt_index == argc {
            // Set `wopt_index` to point at the skipped non-options so that the
            // caller knows where to begin.
            if self.first_nonopt != self.last_nonopt {
                self.wopt_index = self.first_nonopt;
            }

            return Err(None);
        }

        // If we find a non-option and don't permute it, either stop the scan or describe
        // it to the caller and pass it by.
        if self.argv[self.wopt_index].char_at(0) != '-' || self.argv[self.wopt_index].len() == 1 {
            if self.ordering == Ordering::RequireOrder {
                return Err(None);
            }

            self.woptarg = Some(self.argv[self.wopt_index]);
            self.wopt_index += 1;
            return Err(Some(NON_OPTION_CHAR));
        }

        // We've found an option, so we need to skip the initial punctuation.
        let skip = if !self.longopts.is_empty() && self.argv[self.wopt_index].char_at(1) == '-' {
            2
        } else {
            1
        };

        self.remaining_text = self.argv[self.wopt_index][skip..].into();
        Ok(())
    }

    /// Check for a matching short-named option.
    fn handle_short_opt(&mut self) -> char {
        // Look at and handle the next short-named option
        let mut c = self.remaining_text.char_at(0);
        self.remaining_text = &self.remaining_text[1..];

        let temp = self
            .shortopts
            .chars()
            .position(|sc| sc == c)
            .map_or(L!(""), |pos| &self.shortopts[pos..]);

        // Increment `wopt_index' when we run out of text.
        if self.remaining_text.is_empty() {
            self.wopt_index += 1;
        }

        if temp.is_empty() || c == ':' {
            self.unrecognized_opt = c;
            if !self.remaining_text.is_empty() {
                self.wopt_index += 1;
            }
            return '?';
        }

        if temp.char_at(1) != ':' {
            return c;
        }

        if temp.char_at(2) == ':' {
            // This option accepts an optional argument.
            if !self.remaining_text.is_empty() {
                // Consume the remaining text.
                self.woptarg = Some(self.remaining_text);
                self.wopt_index += 1;
            } else {
                self.woptarg = None;
            }
        } else {
            // This option requires an argument.
            if !self.remaining_text.is_empty() {
                // Consume the remaining text.
                self.woptarg = Some(self.remaining_text);
                self.wopt_index += 1;
            } else if self.wopt_index == self.argv.len() {
                // If there's nothing in `remaining_text` and there's
                // no following element to consume, then the option
                // has no argument.
                self.unrecognized_opt = c;
                c = if self.return_colon { ':' } else { '?' };
            } else {
                // Consume the next element.
                self.woptarg = Some(self.argv[self.wopt_index]);
                self.wopt_index += 1;
            }
        }

        self.remaining_text = empty_wstr();
        c
    }

    fn update_long_opt(
        &mut self,
        opt_found: &WOption,
        name_end: usize,
        longopt_index: &mut usize,
        option_index: usize,
    ) -> char {
        self.wopt_index += 1;
        assert!(matches!(self.remaining_text.char_at(name_end), '\0' | '='));

        if self.remaining_text.char_at(name_end) == '=' {
            if opt_found.has_arg != ArgType::NoArgument {
                self.woptarg = Some(self.remaining_text[(name_end + 1)..].into());
            } else {
                self.remaining_text = empty_wstr();
                return '?';
            }
        } else if opt_found.has_arg == ArgType::RequiredArgument {
            if self.wopt_index < self.argv.len() {
                self.woptarg = Some(self.argv[self.wopt_index]);
                self.wopt_index += 1;
            } else {
                self.remaining_text = empty_wstr();
                return if self.return_colon { ':' } else { '?' };
            }
        }

        self.remaining_text = empty_wstr();
        *longopt_index = option_index;
        opt_found.val
    }

    /// Find a matching long-named option.
    fn find_matching_long_opt(&self, name_end: usize) -> LongOptMatch<'opts> {
        let mut opt = None;
        let mut index = 0;
        let mut exact = false;
        let mut ambiguous = false;

        // Test all long options for either exact match or abbreviated matches.
        for (i, potential_match) in self.longopts.iter().enumerate() {
            // Check if current option is prefix of long opt
            if potential_match
                .name
                .starts_with(&self.remaining_text[..name_end])
            {
                if name_end == potential_match.name.len() {
                    // The option matches the text exactly, so we're finished.
                    opt = Some(*potential_match);
                    index = i;
                    exact = true;
                    break;
                } else if opt.is_none() {
                    // The option begins with text, but isn't an exact match.
                    opt = Some(*potential_match);
                    index = i;
                } else {
                    // There are multiple options that match the text, so
                    // it's ambiguous.
                    ambiguous = true;
                }
            }
        }

        if let Some(opt) = opt {
            if exact {
                LongOptMatch::Exact(opt, index)
            } else if ambiguous {
                LongOptMatch::Ambiguous
            } else {
                LongOptMatch::NonExact(opt, index)
            }
        } else {
            LongOptMatch::NoMatch
        }
    }

    /// Check for a matching long-named option.
    fn handle_long_opt(&mut self, longopt_index: &mut usize) -> Option<char> {
        let name_end = self
            .remaining_text
            .chars()
            .take_while(|c| !matches!(c, '\0' | '='))
            .count();

        match self.find_matching_long_opt(name_end) {
            LongOptMatch::Exact(opt, index) | LongOptMatch::NonExact(opt, index) => {
                return Some(self.update_long_opt(&opt, name_end, longopt_index, index));
            }
            LongOptMatch::Ambiguous => {
                self.remaining_text = empty_wstr();
                self.wopt_index += 1;
                return Some('?');
            }
            LongOptMatch::NoMatch => {}
        }

        // If we can't find a long option, try to interpret it as a short option.
        // If it isn't a short option either, return an error.
        if self.argv[self.wopt_index].char_at(1) == '-'
            || !self
                .shortopts
                .as_char_slice()
                .contains(&self.remaining_text.char_at(0))
        {
            self.remaining_text = empty_wstr();
            self.wopt_index += 1;

            return Some('?');
        }

        None
    }

    /// `longopt_index` contains the index of the most recent long-named option
    /// found by the most recent call. Returns the next short-named option if
    /// found.
    fn wgetopt_inner(&mut self, longopt_index: &mut usize) -> Option<char> {
        if !self.initialized {
            self.initialize();
        }

        self.woptarg = None;
        if self.remaining_text.is_empty() {
            if let Err(narg) = self.next_argv() {
                return narg;
            }
        }

        // We set things up so that `-f` is parsed as a short-named option if there
        // is a valid option to match it to, otherwise we parse it as a long-named
        // option. We also make sure that `-fu` is *not* parsed as `-f` with
        // an arg `u`.
        if !self.longopts.is_empty() && self.wopt_index < self.argv.len() {
            let arg = self.argv[self.wopt_index];

            let try_long =
                // matches options like `--foo`
                arg.char_at(0) == '-' && arg.char_at(1) == '-'
                // matches options like `-f` if `f` is not a valid shortopt.
                || !self.shortopts.as_char_slice().contains(&arg.char_at(1));

            if try_long {
                let retval = self.handle_long_opt(longopt_index);
                if retval.is_some() {
                    return retval;
                }
            }
        }

        Some(self.handle_short_opt())
    }
}
