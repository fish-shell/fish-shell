use pcre2::utf32::{Regex, RegexBuilder};
use std::borrow::Cow;

use super::*;
use crate::future_feature_flags::{feature_test, FeatureFlag};

#[derive(Default)]
pub struct Replace<'args> {
    all: bool,
    filter: bool,
    ignore_case: bool,
    quiet: bool,
    regex: bool,
    pattern: &'args wstr,
    replacement: &'args wstr,
}

impl<'args> StringSubCommand<'args> for Replace<'args> {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            wopt(L!("all"), no_argument, 'a'),
            wopt(L!("filter"), no_argument, 'f'),
            wopt(L!("ignore-case"), no_argument, 'i'),
            wopt(L!("quiet"), no_argument, 'q'),
            wopt(L!("regex"), no_argument, 'r'),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":afiqr")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'a' => self.all = true,
            'f' => self.filter = true,
            'i' => self.ignore_case = true,
            'q' => self.quiet = true,
            'r' => self.regex = true,
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &'args [WString],
        streams: &mut IoStreams<'_>,
    ) -> Option<libc::c_int> {
        let cmd = &args[0];
        let Some(pattern) = args.get(*optind) else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT0, cmd);
            return STATUS_INVALID_ARGS;
        };
        *optind += 1;
        let Some(replacement) = args.get(*optind) else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT1, cmd, 1, 2);
            return STATUS_INVALID_ARGS;
        };
        *optind += 1;

        self.pattern = pattern;
        self.replacement = replacement;
        return STATUS_CMD_OK;
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams<'_>,
        optind: &mut usize,
        args: &[WString],
    ) -> Option<libc::c_int> {
        let cmd = &args[0];

        let replacer = match StringReplacer::new(self.pattern, self.replacement, self) {
            Ok(x) => x,
            Err(e) => {
                e.print_error(args, streams);
                return STATUS_INVALID_ARGS;
            }
        };

        let mut replace_count = 0;

        for (arg, want_newline) in arguments(args, optind, streams) {
            let (replaced, result) = match replacer.replace(arg) {
                Ok(x) => x,
                Err(e) => {
                    string_error!(
                        streams,
                        "%ls: Regular expression substitute error: %ls\n",
                        cmd,
                        e.error_message()
                    );
                    return STATUS_INVALID_ARGS;
                }
            };
            replace_count += replaced as usize;

            if !self.quiet && (!self.filter || replaced) {
                streams.out.append(&result);
                if want_newline {
                    streams.out.append1('\n');
                }
            }

            if self.quiet && replace_count > 0 {
                return STATUS_CMD_OK;
            }
        }

        if replace_count > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

#[allow(clippy::large_enum_variant)]
enum StringReplacer<'args, 'opts> {
    Regex {
        replacement: WString,
        regex: Regex,
        opts: &'opts Replace<'args>,
    },
    Literal {
        pattern: Cow<'args, wstr>,
        replacement: Cow<'args, wstr>,
        opts: &'opts Replace<'args>,
    },
}

impl<'args, 'opts> StringReplacer<'args, 'opts> {
    fn interpret_escape(arg: &'args wstr) -> Option<WString> {
        use crate::common::read_unquoted_escape;

        let mut result: WString = WString::with_capacity(arg.len());
        let mut cursor = arg;
        while !cursor.is_empty() {
            if cursor.char_at(0) == '\\' {
                if let Some(escape_len) = read_unquoted_escape(cursor, &mut result, true, false) {
                    cursor = cursor.slice_from(escape_len);
                } else {
                    // invalid escape
                    return None;
                }
            } else {
                result.push(cursor.char_at(0));
                cursor = cursor.slice_from(1);
            }
        }
        return Some(result);
    }

    fn new(
        pattern: &'args wstr,
        replacement: &'args wstr,
        opts: &'opts Replace<'args>,
    ) -> Result<Self, RegexError> {
        let r = match (opts.regex, opts.ignore_case) {
            (true, _) => {
                let regex = RegexBuilder::new()
                    .caseless(opts.ignore_case)
                    // set to behave similarly to match, could probably be either enabled by default or
                    // allowed to be user-controlled here
                    .never_utf(true)
                    .build(pattern.as_char_slice())
                    .map_err(|e| RegexError::Compile(pattern.to_owned(), e))?;

                let replacement = if feature_test(FeatureFlag::string_replace_backslash) {
                    replacement.to_owned()
                } else {
                    Self::interpret_escape(replacement)
                        .ok_or_else(|| RegexError::InvalidEscape(pattern.to_owned()))?
                };
                Self::Regex {
                    replacement,
                    regex,
                    opts,
                }
            }
            (false, true) => Self::Literal {
                // previously we used wcsncasecmp but there is no equivalent function in Rust widestring
                // this should likely be handled by a using the `literal` option on our regex
                pattern: Cow::Owned(pattern.to_lowercase()),
                replacement: Cow::Owned(replacement.to_owned()),
                opts,
            },
            (false, false) => Self::Literal {
                pattern: Cow::Borrowed(pattern),
                replacement: Cow::Borrowed(replacement),
                opts,
            },
        };
        Ok(r)
    }

    fn replace<'a>(&self, arg: WString) -> Result<(bool, WString), pcre2::Error> {
        match self {
            StringReplacer::Regex {
                replacement,
                regex,
                opts,
            } => {
                let res = if opts.all {
                    regex.replace_all(arg.as_char_slice(), replacement.as_char_slice(), true)
                } else {
                    regex.replace(arg.as_char_slice(), replacement.as_char_slice(), true)
                }?;

                let res = match res {
                    Cow::Borrowed(_slice_of_arg) => (false, arg),
                    Cow::Owned(s) => (true, WString::from_chars(s)),
                };
                return Ok(res);
            }
            StringReplacer::Literal {
                pattern,
                replacement,
                opts,
            } => {
                if pattern.is_empty() {
                    return Ok((false, arg));
                }

                // a premature optimization would be to alloc larger if we have replacement.len() > pattern.len()
                let mut result = WString::with_capacity(arg.len());

                let subject = if opts.ignore_case {
                    arg.to_lowercase()
                } else {
                    arg.clone()
                };

                let mut offset = 0;
                while let Some(idx) = subject[offset..].find(pattern.as_char_slice()) {
                    result.push_utfstr(&subject[offset..offset + idx]);
                    result.push_utfstr(&replacement);
                    offset += idx + pattern.len();
                    if !opts.all {
                        break;
                    }
                }
                if offset == 0 {
                    return Ok((false, arg));
                }
                result.push_utfstr(&arg[offset..]);

                Ok((true, result))
            }
        }
    }
}
