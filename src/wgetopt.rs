//! A version of the getopt library for use with wide character strings, rewritten in
//! Rust.
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

/// Special char used with [`Ordering::ReturnInOrder`].
pub const NON_OPTION_CHAR: char = '\x01';

/// Utility function to quickly return a reference to an empty wstr.
fn empty_wstr() -> &'static wstr {
    Default::default()
}

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
    /// The elements in `argv` are reordered so that non-option arguments end up
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

/// Indicates whether an option takes an argument, and whether that argument
/// is optional.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ArgType {
    /// The option takes no arguments.
    #[default]
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
    pub arg_type: ArgType,
    /// If the option is found during scanning, this value will be returned to identify it.
    pub val: char,
}

/// Helper function to create a `WOption`.
pub const fn wopt(name: &wstr, arg_type: ArgType, val: char) -> WOption<'_> {
    WOption {
        name,
        arg_type,
        val,
    }
}

/// Used internally by [WGetopter::find_matching_long_opt]. See there for further
/// details.
#[derive(Default)]
enum LongOptMatch<'a> {
    Exact(WOption<'a>, usize),
    NonExact(WOption<'a>, usize),
    Ambiguous,
    #[default]
    NoMatch,
}

/// Used internally by [WGetopter::next_argv]. See there for further details.
enum NextArgv {
    Finished,
    UnpermutedNonOption,
    FoundOption,
}

use std::borrow::Cow;

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
    /// Prevents redundant initialization.
    initialized: bool,
    // Makes parsing long options more strict (long options must have their full name specified,
    // and cannot be given with a single hyphen).
    pub strict_long_opts: bool,
    /// This will be populated with the elements of the original args that were interpreted
    /// as options and arguments to options
    pub argv_opts: Vec<Cow<'args, wstr>>,
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
            initialized: false,
            strict_long_opts: false,
            argv_opts: Vec::new(),
        }
    }

    /// Try to get the next option, returning:
    /// * None if there are no more options
    /// * `Some(`[`NON_OPTION_CHAR`]`)` for a non-option when using [`Ordering::ReturnInOrder`]
    /// * `Some('?') for unrecognised options
    /// * `Some(':')` for options missing an argument,
    /// * `Some(';') for options with an unexpected argument (this is only possible when using the
    ///   --long=value or -long=value syntax where long was declared as taking no arguments).
    /// * Otherwise, `Some(c)`, where `c` is the option's short character
    pub fn next_opt(&mut self) -> Option<char> {
        assert!(
            self.wopt_index <= self.argv.len(),
            "wopt_index is out of range"
        );

        let mut ignored = 0;
        self.wgetopt_inner(&mut ignored)
    }

    // Tries to get the next option, additionally returning the index of the long option
    // if found.
    pub fn next_opt_indexed(&mut self) -> Option<(char, Option<usize>)> {
        let mut longopt_index = usize::MAX;
        let option = self.wgetopt_inner(&mut longopt_index);
        if longopt_index != usize::MAX {
            option.map(|c| (c, Some(longopt_index)))
        } else {
            option.map(|c| (c, None))
        }
    }

    /// Swaps two subsequences in `argv`, one which contains all non-options skipped
    /// during scanning (defined by the range `[first_nonopt, last_nonopt)`), and
    /// the other containing all options scanned after (defined by the range
    /// `[last_nonopt, wopt_index)`).
    ///
    /// Elements are then swapped between the list so that all options end up
    /// in the former list, and non-options in the latter.
    fn exchange(&mut self) {
        let left = self.first_nonopt;
        let middle = self.last_nonopt;
        let right = self.wopt_index;
        debug_assert!(left <= middle && middle <= right, "Indexes out of order");
        self.argv[left..right].rotate_left(middle - left);

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

        self.shortopts = optstring;
        self.initialized = true;
    }

    /// Advance to the next element in `argv`.
    fn next_argv(&mut self) -> NextArgv {
        let argc = self.argv.len();

        if self.ordering == Ordering::Permute {
            // Permute the args if we've found options following non-options.
            if self.last_nonopt != self.wopt_index {
                if self.first_nonopt == self.last_nonopt {
                    self.first_nonopt = self.wopt_index;
                } else {
                    self.exchange();
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

            return NextArgv::Finished;
        }

        // If we find a non-option and don't permute it, either stop the scan or signal
        // it to the caller and pass it by.
        if self.argv[self.wopt_index].char_at(0) != '-' || self.argv[self.wopt_index].len() == 1 {
            if self.ordering == Ordering::RequireOrder {
                return NextArgv::Finished;
            }

            self.woptarg = Some(self.argv[self.wopt_index]);
            self.wopt_index += 1;
            return NextArgv::UnpermutedNonOption;
        }

        let opt = self.argv[self.wopt_index];
        self.argv_opts.push(Cow::Borrowed(opt));

        // We've found an option, so we need to skip the initial punctuation.
        let skip = if !self.longopts.is_empty() && opt.char_at(1) == '-' {
            2
        } else {
            1
        };

        self.remaining_text = opt[skip..].into();
        NextArgv::FoundOption
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
                c = ':';
            } else {
                // Consume the next element.
                let val = self.argv[self.wopt_index];
                self.argv_opts.push(Cow::Borrowed(val));
                self.woptarg = Some(val);
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
            if opt_found.arg_type == ArgType::NoArgument {
                self.remaining_text = empty_wstr();
                return ';';
            } else {
                self.woptarg = Some(self.remaining_text[(name_end + 1)..].into());
            }
        } else if opt_found.arg_type == ArgType::RequiredArgument {
            if self.wopt_index < self.argv.len() {
                let val = self.argv[self.wopt_index];
                self.argv_opts.push(Cow::Borrowed(val));
                self.woptarg = Some(val);
                self.wopt_index += 1;
            } else {
                self.remaining_text = empty_wstr();
                return ':';
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
                } else if self.strict_long_opts {
                    // It didn't match exactly, continue
                    continue;
                } else if opt.is_none() {
                    // The option begins with the matching text, but is not
                    // exactly equal.
                    opt = Some(*potential_match);
                    index = i;
                } else {
                    // There are multiple options that match the text non-exactly.
                    ambiguous = true;
                }
            }
        }

        opt.map_or(LongOptMatch::NoMatch, |opt| {
            if exact {
                LongOptMatch::Exact(opt, index)
            } else if ambiguous {
                LongOptMatch::Ambiguous
            } else {
                LongOptMatch::NonExact(opt, index)
            }
        })
    }

    /// Check for a matching long-named option.
    fn handle_long_opt(&mut self, longopt_index: &mut usize) -> Option<char> {
        let name_end = if let Some(index) = self.remaining_text.find(['=']) {
            index
        } else {
            self.remaining_text.len()
        };

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
            // Unknown option, assume remaining text is its argument
            // (it needs to be saved somewhere for argparse, but we cant save it in
            // self.remaining_text as that will break further parsing)
            self.woptarg = Some(&self.remaining_text[1..]);
            self.remaining_text = empty_wstr();
            self.wopt_index += 1;

            return Some('?');
        }

        None
    }

    /// Goes through `argv` to try and find options.
    ///
    /// Any element that begins with `-` or `--` and is not just `-` or `--` is an option
    /// element. The characters of this element (aside from the initial `-` or `--`) are
    /// option characters. Repeated calls return each option character successively.
    ///
    /// Options that begin with `--` are long-named. Long-named options can be abbreviated
    /// so long as the abbreviation is unique or is an exact match for some defined option.
    ///
    /// Arguments to options follow their option name, optionally separated by an `=`.
    ///
    /// `longopt_index` contains the index of the most recent long-named option
    /// found by the most recent call. Returns the next short-named option if
    /// found.
    fn wgetopt_inner(&mut self, longopt_index: &mut usize) -> Option<char> {
        if !self.initialized {
            self.initialize();
        }

        self.woptarg = None;
        if self.remaining_text.is_empty() {
            match self.next_argv() {
                NextArgv::UnpermutedNonOption => return Some(NON_OPTION_CHAR),
                NextArgv::Finished => return None,
                NextArgv::FoundOption => (),
            }
        }

        // We set things up so that `-f` is parsed as a short-named option if there
        // is a valid option to match it to, otherwise, if self.strict_long_opts is false,
        // we parse it as a long-named option. We also make sure that `-fu` is *not* parsed as
        // `-f` with an arg `u`.
        if !self.longopts.is_empty() && self.wopt_index < self.argv.len() {
            let arg = self.argv[self.wopt_index];

            let try_long =
                // matches options like `--foo`
                arg.char_at(0) == '-' && arg.char_at(1) == '-'
                // matches options like `-f` if `f` is not a valid shortopt.
                || (!self.strict_long_opts
                    && !self.shortopts.as_char_slice().contains(&arg.char_at(1)));

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

#[cfg(test)]
mod tests {
    use super::{ArgType, WGetopter, WOption, wopt};
    use crate::wchar::prelude::*;
    use crate::wcstringutil::join_strings;

    #[test]
    fn test_exchange() {
        let base_argv = [
            L!("0"),
            L!("1"),
            L!("2"),
            L!("3"),
            L!("4"),
            L!("5"),
            L!("6"),
        ];
        let argc = base_argv.len();
        for start in 0..=argc {
            for mid in start..=argc {
                for end in mid..=argc {
                    let mut argv: Vec<&wstr> = base_argv.to_vec();
                    // After exchange, we expect the start..mid and mid..end ranges to be swapped.
                    let mut expected = argv[mid..end].to_vec();
                    expected.extend(argv[start..mid].iter());

                    let mut w = WGetopter::new(L!(""), &[], &mut argv);

                    w.first_nonopt = start;
                    w.last_nonopt = mid;
                    w.wopt_index = end;
                    w.exchange();

                    // Non-options were permuted to the end.
                    let options_scanned = end - mid;
                    assert_eq!(w.first_nonopt, start + options_scanned);
                    assert_eq!(w.last_nonopt, mid + options_scanned);
                    assert_eq!(w.wopt_index, end);
                    assert_eq!(&w.argv[start..end], expected);
                }
            }
        }
    }

    #[test]
    fn test_wgetopt() {
        // Regression test for a crash.
        const short_options: &wstr = L!("-a");
        const long_options: &[WOption] = &[wopt(L!("add"), ArgType::NoArgument, 'a')];
        let mut argv = [
            L!("abbr"),
            L!("--add"),
            L!("emacsnw"),
            L!("emacs"),
            L!("-nw"),
        ];
        let mut w = WGetopter::new(short_options, long_options, &mut argv);
        let mut a_count = 0;
        let mut arguments = vec![];
        while let Some(opt) = w.next_opt() {
            match opt {
                'a' => {
                    a_count += 1;
                }
                '\x01' => {
                    // non-option argument
                    arguments.push(w.woptarg.as_ref().unwrap().to_owned());
                }
                '?' => {
                    // unrecognized option
                    if let Some(arg) = w.argv.get(w.wopt_index - 1) {
                        arguments.push(arg.to_owned());
                    }
                }
                _ => {
                    panic!("unexpected option: {:?}", opt);
                }
            }
        }
        assert_eq!(a_count, 1);
        assert_eq!(arguments.len(), 3);
        assert_eq!(join_strings(&arguments, ' '), "emacsnw emacs -nw");
    }
}
