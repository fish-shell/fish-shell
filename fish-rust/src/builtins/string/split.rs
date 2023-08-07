use std::borrow::Cow;
use std::ops::Deref;

use super::*;
use crate::wcstringutil::split_about;
use crate::wutil::{fish_wcstoi, fish_wcstol};

pub struct Split<'args> {
    quiet: bool,
    split_from: Direction,
    max: usize,
    no_empty: bool,
    fields: Fields,
    allow_empty: bool,
    pub is_split0: bool,
    sep: &'args wstr,
}

impl Default for Split<'_> {
    fn default() -> Self {
        Self {
            quiet: false,
            split_from: Direction::Left,
            max: usize::MAX,
            no_empty: false,
            fields: Fields(Vec::new()),
            allow_empty: false,
            is_split0: false,
            sep: L!("\0"),
        }
    }
}

#[repr(transparent)]
struct Fields(Vec<usize>);

// we have a newtype just for the sake of implementing TryFrom
impl Deref for Fields {
    type Target = Vec<usize>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

enum FieldParseError {
    /// Unable to parse as integer
    Number,
    /// One of the ends in a range is either too big or small
    Range,
    /// The field is a valid number but outside of the allowed range
    Field,
}

impl From<crate::wutil::wcstoi::Error> for FieldParseError {
    fn from(_: crate::wutil::wcstoi::Error) -> Self {
        FieldParseError::Number
    }
}

impl<'args> TryFrom<&'args wstr> for Fields {
    type Error = FieldParseError;

    /// FIELDS is a comma-separated string of field numbers and/or spans.
    /// Each field is one-indexed.
    fn try_from(value: &wstr) -> Result<Self, Self::Error> {
        fn parse_field(f: &wstr) -> Result<Vec<usize>, FieldParseError> {
            use FieldParseError::*;
            let range: Vec<&wstr> = f.split('-').collect();
            let range: Vec<usize> = match range[..] {
                [s, e] => {
                    let start = fish_wcstoi(s)? as usize;
                    let end = fish_wcstoi(e)? as usize;

                    if start == 0 || end == 0 {
                        return Err(Range);
                    }

                    if start <= end {
                        // we store as 0-indexed, but the range is 1-indexed
                        (start - 1..end).collect()
                    } else {
                        // this is allowed
                        (end - 1..start).rev().collect()
                    }
                }
                _ => match fish_wcstoi(f)? as usize {
                    n @ 1.. => vec![n - 1],
                    _ => return Err(Field),
                },
            };
            Ok(range)
        }

        let fields = value.split(',').map(parse_field);

        let mut indices = Vec::new();
        for field in fields {
            indices.extend(field?);
        }

        Ok(Self(indices))
    }
}

impl<'args> StringSubCommand<'args> for Split<'args> {
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        wopt(L!("quiet"), no_argument, 'q'),
        wopt(L!("right"), no_argument, 'r'),
        wopt(L!("max"), required_argument, 'm'),
        wopt(L!("no-empty"), no_argument, 'n'),
        wopt(L!("fields"), required_argument, 'f'),
        // FIXME: allow-empty is not documented
        wopt(L!("allow-empty"), no_argument, 'a'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":qrm:nf:a");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'q' => self.quiet = true,
            'r' => self.split_from = Direction::Right,
            'm' => {
                self.max = fish_wcstol(arg.unwrap())?
                    .try_into()
                    .map_err(|_| invalid_args!("%ls: Invalid max value '%ls'\n", name, arg))?
            }
            'n' => self.no_empty = true,
            'f' => {
                self.fields = arg.unwrap().try_into().map_err(|e| match e {
                    FieldParseError::Number => StringError::NotANumber,
                    FieldParseError::Range => {
                        invalid_args!("%ls: Invalid range value for field '%ls'\n", name, arg)
                    }
                    FieldParseError::Field => {
                        invalid_args!("%ls: Invalid fields value '%ls'\n", name, arg)
                    }
                })?;
            }
            'a' => self.allow_empty = true,
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'args wstr],
        streams: &mut io_streams_t,
    ) -> Option<libc::c_int> {
        if self.is_split0 {
            return STATUS_CMD_OK;
        }
        let Some(arg) = args.get(*optind).copied() else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT0, args[0]);
            return STATUS_INVALID_ARGS;
        };
        *optind += 1;
        self.sep = arg;
        return STATUS_CMD_OK;
    }

    fn handle(
        &mut self,
        _parser: &mut parser_t,
        streams: &mut io_streams_t,
        optind: &mut usize,
        args: &[&'args wstr],
    ) -> Option<libc::c_int> {
        if self.fields.is_empty() && self.allow_empty {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                args[0],
                wgettext!("--allow-empty is only valid with --fields")
            ));
            return STATUS_INVALID_ARGS;
        }

        let sep = self.sep;
        let mut all_splits: Vec<Vec<Cow<'args, wstr>>> = Vec::new();
        let mut split_count = 0usize;
        let mut arg_count = 0usize;

        let argiter = arguments(args, optind, streams).with_split_behavior(match self.is_split0 {
            false => SplitBehavior::Newline,
            true => SplitBehavior::Never,
        });
        for (arg, _) in argiter {
            let splits: Vec<Cow<'args, wstr>> = match (self.split_from, arg) {
                (Direction::Right, arg) => {
                    let mut rev = arg.into_owned();
                    rev.as_char_slice_mut().reverse();
                    let sep: WString = sep.chars().rev().collect();
                    split_about(&rev, &sep, self.max, self.no_empty)
                        .into_iter()
                        // If we are from the right, split_about gave us reversed strings, in reversed order!
                        .map(|s| Cow::Owned(s.chars().rev().collect::<WString>()))
                        .rev()
                        .collect()
                }
                // we need to special-case the Cow::Borrowed case, since
                // let arg: &'args wstr = &arg;
                // does not compile since `arg` can be dropped at the end of this scope
                // making the reference invalid if it is owned.
                (Direction::Left, Cow::Borrowed(arg)) => {
                    split_about(arg, sep, self.max, self.no_empty)
                        .into_iter()
                        .map(Cow::Borrowed)
                        .collect()
                }
                (Direction::Left, Cow::Owned(arg)) => {
                    split_about(&arg, sep, self.max, self.no_empty)
                        .into_iter()
                        .map(|s| Cow::Owned(s.to_owned()))
                        .collect()
                }
            };

            // If we're quiet, we return early if we've found something to split.
            if self.quiet && splits.len() > 1 {
                return STATUS_CMD_OK;
            }
            split_count += splits.len();
            arg_count += 1;
            all_splits.push(splits);
        }

        if self.quiet {
            return if split_count > arg_count {
                STATUS_CMD_OK
            } else {
                STATUS_CMD_ERROR
            };
        }

        for mut splits in all_splits {
            if self.is_split0 && !splits.is_empty() {
                // split0 ignores a trailing \0, so a\0b\0 is two elements.
                // In contrast to split, where a\nb\n is three - "a", "b" and "".
                //
                // Remove the last element if it is empty.
                if splits.last().unwrap().is_empty() {
                    splits.pop();
                }
            }

            let splits = splits;

            if !self.fields.is_empty() {
                // Print nothing and return error if any of the supplied
                // fields do not exist, unless `--allow-empty` is used.
                if !self.allow_empty {
                    for field in self.fields.iter() {
                        // we already have checked the start
                        if *field >= splits.len() {
                            return STATUS_CMD_ERROR;
                        }
                    }
                }
                for field in self.fields.iter() {
                    if let Some(val) = splits.get(*field) {
                        streams.out.append_with_separation(
                            val,
                            separation_type_t::explicitly,
                            true,
                        );
                    }
                }
            } else {
                for split in &splits {
                    streams
                        .out
                        .append_with_separation(split, separation_type_t::explicitly, true);
                }
            }
        }

        // We split something if we have more split values than args.
        return if split_count > arg_count {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        };
    }
}
