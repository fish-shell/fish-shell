use std::num::NonZeroUsize;

use pcre2::utf32::{Regex, RegexBuilder};

use super::*;
use crate::future_feature_flags::{FeatureFlag, feature_test};

#[derive(Default)]
pub struct Replace<'args> {
    all: bool,
    filter: bool,
    ignore_case: bool,
    quiet: bool,
    regex: bool,
    pattern: &'args wstr,
    replacement: &'args wstr,
    max_matches: Option<NonZeroUsize>,
}

impl<'args> StringSubCommand<'args> for Replace<'args> {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("all"), NoArgument, 'a'),
        wopt(L!("filter"), NoArgument, 'f'),
        wopt(L!("ignore-case"), NoArgument, 'i'),
        wopt(L!("quiet"), NoArgument, 'q'),
        wopt(L!("regex"), NoArgument, 'r'),
        wopt(L!("max-matches"), RequiredArgument, 'm'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("afiqrm:");

    fn parse_opt(&mut self, _n: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'a' => self.all = true,
            'f' => self.filter = true,
            'i' => self.ignore_case = true,
            'q' => self.quiet = true,
            'r' => self.regex = true,
            'm' => {
                self.max_matches = {
                    let arg = arg.expect("Option -m requires a non-zero argument");
                    let max = fish_wcstoul(arg)
                        .ok()
                        .and_then(|v| NonZeroUsize::new(v as usize))
                        .ok_or_else(|| {
                            StringError::InvalidArgs(wgettext_fmt!(
                                BUILTIN_ERR_INVALID_MAX_MATCHES,
                                _n,
                                arg
                            ))
                        })?;
                    Some(max)
                }
            }
            _ => return Err(StringError::UnknownOption),
        }
        Ok(())
    }

    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'args wstr],
        streams: &mut IoStreams,
    ) -> Result<(), ErrorCode> {
        let cmd = args[0];
        let Some(pattern) = args.get(*optind).copied() else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT0, cmd);
            return Err(STATUS_INVALID_ARGS);
        };
        *optind += 1;
        let Some(replacement) = args.get(*optind).copied() else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT1, cmd, 1, 2);
            return Err(STATUS_INVALID_ARGS);
        };
        *optind += 1;

        self.pattern = pattern;
        self.replacement = replacement;
        Ok(())
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Result<(), ErrorCode> {
        let cmd = args[0];

        let replacer = match StringReplacer::new(self.pattern, self.replacement, self) {
            Ok(x) => x,
            Err(e) => {
                e.print_error(args, streams);
                return Err(STATUS_INVALID_ARGS);
            }
        };

        let mut replace_count = 0;

        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            let (replaced, result) = match replacer.replace(arg) {
                Ok(x) => x,
                Err(e) => {
                    string_error!(
                        streams,
                        "%s: Regular expression substitute error: %s",
                        cmd,
                        e.error_message()
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
            };
            replace_count += replaced as usize;

            if !self.quiet && (!self.filter || replaced) {
                streams.out.append(&result);
                if want_newline {
                    streams.out.append('\n');
                }
            }

            if self.quiet && replace_count > 0 {
                return Ok(());
            }
            if self
                .max_matches
                .is_some_and(|max| max.get() == replace_count)
            {
                return Ok(());
            }
        }

        if replace_count > 0 {
            Ok(())
        } else {
            Err(STATUS_CMD_ERROR)
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
        Some(result)
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
                    .block_utf_pattern_directive(true)
                    .build(pattern.as_char_slice())
                    .map_err(|e| RegexError::Compile(pattern.to_owned(), e))?;

                let replacement = if feature_test(FeatureFlag::StringReplaceBackslash) {
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

    fn replace<'a>(&self, arg: Cow<'a, wstr>) -> Result<(bool, Cow<'a, wstr>), pcre2::Error> {
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
                    Cow::Owned(s) => (true, Cow::Owned(WString::from_chars(s))),
                };
                Ok(res)
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
                    arg.as_ref().to_owned()
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

                Ok((true, Cow::Owned(result)))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::builtins::shared::{STATUS_CMD_ERROR, STATUS_CMD_OK, STATUS_INVALID_ARGS};
    use crate::tests::prelude::*;
    use crate::validate;

    #[test]
    #[serial]
    #[rustfmt::skip]
    fn plain() {
        let _cleanup = test_init();
        validate!(["string", "replace", ""], STATUS_INVALID_ARGS, "");
        validate!(["string", "replace", "", ""], STATUS_CMD_ERROR, "");
        validate!(["string", "replace", "", "", ""], STATUS_CMD_ERROR, "\n");
        validate!(["string", "replace", "", "", " "], STATUS_CMD_ERROR, " \n");
        validate!(["string", "replace", "a", "b", ""], STATUS_CMD_ERROR, "\n");
        validate!(["string", "replace", "a", "b", "a"], STATUS_CMD_OK, "b\n");
        validate!(["string", "replace", "a", "b", "xax"], STATUS_CMD_OK, "xbx\n");
        validate!(["string", "replace", "a", "b", "xax", "axa"], STATUS_CMD_OK, "xbx\nbxa\n");
        validate!(["string", "replace", "bar", "x", "red barn"], STATUS_CMD_OK, "red xn\n");
        validate!(["string", "replace", "x", "bar", "red xn"], STATUS_CMD_OK, "red barn\n");
        validate!(["string", "replace", "--", "x", "-", "xyz"], STATUS_CMD_OK, "-yz\n");
        validate!(["string", "replace", "--", "y", "-", "xyz"], STATUS_CMD_OK, "x-z\n");
        validate!(["string", "replace", "--", "z", "-", "xyz"], STATUS_CMD_OK, "xy-\n");
        validate!(["string", "replace", "-i", "z", "X", "_Z_"], STATUS_CMD_OK, "_X_\n");
        validate!(["string", "replace", "-a", "a", "A", "aaa"], STATUS_CMD_OK, "AAA\n");
        validate!(["string", "replace", "-i", "a", "z", "AAA"], STATUS_CMD_OK, "zAA\n");
        validate!(["string", "replace", "-q", "x", ">x<", "x"], STATUS_CMD_OK, "");
        validate!(["string", "replace", "-a", "x", "", "xxx"], STATUS_CMD_OK, "\n");
        validate!(["string", "replace", "-a", "***", "_", "*****"], STATUS_CMD_OK, "_**\n");
        validate!(["string", "replace", "-a", "***", "***", "******"], STATUS_CMD_OK, "******\n");
        validate!(["string", "replace", "-a", "a", "b", "xax", "axa"], STATUS_CMD_OK, "xbx\nbxb\n");

        validate!(["string", "replace", "-r"], STATUS_INVALID_ARGS, "");
        validate!(["string", "replace", "-r", ""], STATUS_INVALID_ARGS, "");
        validate!(["string", "replace", "-r", "", ""], STATUS_CMD_ERROR, "");
        validate!(["string", "replace", "-r", "", "", ""], STATUS_CMD_OK, "\n");  // pcre2 behavior
        validate!(["string", "replace", "-r", "", "", " "], STATUS_CMD_OK, " \n");  // pcre2 behavior
        validate!(["string", "replace", "-r", "a", "b", ""], STATUS_CMD_ERROR, "\n");
        validate!(["string", "replace", "-r", "a", "b", "a"], STATUS_CMD_OK, "b\n");
        validate!(["string", "replace", "-r", ".", "x", "abc"], STATUS_CMD_OK, "xbc\n");
        validate!(["string", "replace", "-r", ".", "", "abc"], STATUS_CMD_OK, "bc\n");
        validate!(["string", "replace", "-r", "(\\w)(\\w)", "$2$1", "ab"], STATUS_CMD_OK, "ba\n");
        validate!(["string", "replace", "-r", "(\\w)", "$1$1", "ab"], STATUS_CMD_OK, "aab\n");
        validate!(["string", "replace", "-r", "-a", ".", "x", "abc"], STATUS_CMD_OK, "xxx\n");
        validate!(["string", "replace", "-r", "-a", "(\\w)", "$1$1", "ab"], STATUS_CMD_OK, "aabb\n");
        validate!(["string", "replace", "-r", "-a", ".", "", "abc"], STATUS_CMD_OK, "\n");
        validate!(["string", "replace", "-r", "a", "x", "bc", "cd", "de"], STATUS_CMD_ERROR, "bc\ncd\nde\n");
        validate!(["string", "replace", "-r", "a", "x", "aba", "caa"], STATUS_CMD_OK, "xba\ncxa\n");
        validate!(["string", "replace", "-r", "-a", "a", "x", "aba", "caa"], STATUS_CMD_OK, "xbx\ncxx\n");
        validate!(["string", "replace", "-r", "-i", "A", "b", "xax"], STATUS_CMD_OK, "xbx\n");
        validate!(["string", "replace", "-r", "-i", "[a-z]", ".", "1A2B"], STATUS_CMD_OK, "1.2B\n");
        validate!(["string", "replace", "-r", "A", "b", "xax"], STATUS_CMD_ERROR, "xax\n");
        validate!(["string", "replace", "-r", "a", "$1", "a"], STATUS_INVALID_ARGS, "");
        validate!(["string", "replace", "-r", "(a)", "$2", "a"], STATUS_INVALID_ARGS, "");
        validate!(["string", "replace", "-r", "*", ".", "a"], STATUS_INVALID_ARGS, "");
        validate!(["string", "replace", "-ra", "x", "\\c"], STATUS_CMD_ERROR, "");
        validate!(["string", "replace", "-r", "^(.)", "\t$1", "abc", "x"], STATUS_CMD_OK, "\tabc\n\tx\n");
    }
}
