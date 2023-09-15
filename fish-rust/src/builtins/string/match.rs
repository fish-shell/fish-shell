use pcre2::utf32::{Captures, Regex, RegexBuilder};
use printf_compat::sprintf;
use std::collections::HashMap;

use super::*;
use crate::env::{EnvMode, EnvVar, EnvVarFlags};
use crate::flog::FLOG;
use crate::parse_util::parse_util_unescape_wildcards;
use crate::wildcard::{wildcard_match, ANY_STRING};

#[derive(Default)]
pub struct Match<'args> {
    all: bool,
    entire: bool,
    groups_only: bool,
    ignore_case: bool,
    invert_match: bool,
    quiet: bool,
    regex: bool,
    index: bool,
    pattern: &'args wstr,
}

impl<'args> StringSubCommand<'args> for Match<'args> {
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        wopt(L!("all"), no_argument, 'a'),
        wopt(L!("entire"), no_argument, 'e'),
        wopt(L!("groups-only"), no_argument, 'g'),
        wopt(L!("ignore-case"), no_argument, 'i'),
        wopt(L!("invert"), no_argument, 'v'),
        wopt(L!("quiet"), no_argument, 'q'),
        wopt(L!("regex"), no_argument, 'r'),
        wopt(L!("index"), no_argument, 'n'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":aegivqrn");

    fn parse_opt(&mut self, _n: &wstr, c: char, _arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'a' => self.all = true,
            'e' => self.entire = true,
            'g' => self.groups_only = true,
            'i' => self.ignore_case = true,
            'v' => self.invert_match = true,
            'q' => self.quiet = true,
            'r' => self.regex = true,
            'n' => self.index = true,
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
        let cmd = args[0];
        let Some(arg) = args.get(*optind).copied() else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT0, cmd);
            return STATUS_INVALID_ARGS;
        };
        *optind += 1;
        self.pattern = arg;
        STATUS_CMD_OK
    }

    fn handle(
        &mut self,
        parser: &mut parser_t,
        streams: &mut io_streams_t,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Option<libc::c_int> {
        let cmd = args[0];

        if self.entire && self.index {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                cmd,
                wgettext!("--entire and --index are mutually exclusive")
            ));
            return STATUS_INVALID_ARGS;
        }

        if self.invert_match && self.groups_only {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                cmd,
                wgettext!("--invert and --groups-only are mutually exclusive")
            ));
            return STATUS_INVALID_ARGS;
        }

        if self.entire && self.groups_only {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                cmd,
                wgettext!("--entire and --groups-only are mutually exclusive")
            ));
            return STATUS_INVALID_ARGS;
        }

        let mut matcher = match StringMatcher::new(self.pattern, self) {
            Ok(m) => m,
            Err(e) => {
                e.print_error(args, streams);
                return STATUS_INVALID_ARGS;
            }
        };

        for (arg, _) in arguments(args, optind, streams) {
            if let Err(e) = matcher.report_matches(arg.as_ref(), streams) {
                FLOG!(error, "pcre2_match unexpected error:", e.error_message())
            }
            if self.quiet && matcher.match_count() > 0 {
                break;
            }
        }

        let match_count = matcher.match_count();

        if let StringMatcher::Regex(RegexMatcher {
            first_match_captures,
            ..
        }) = matcher
        {
            let vars = parser.get_vars();
            for (name, vals) in first_match_captures.into_iter() {
                vars.set(&WString::from(name), EnvMode::default(), vals);
            }
        }

        if match_count > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

struct RegexMatcher<'opts, 'args> {
    regex: Regex,
    total_matched: usize,
    first_match_captures: HashMap<String, Vec<WString>>,
    opts: &'opts Match<'args>,
}

struct WildCardMatcher<'opts, 'args> {
    pattern: WString,
    total_matched: usize,
    opts: &'opts Match<'args>,
}

#[allow(clippy::large_enum_variant)]
enum StringMatcher<'opts, 'args> {
    Regex(RegexMatcher<'opts, 'args>),
    WildCard(WildCardMatcher<'opts, 'args>),
}

impl<'opts, 'args> StringMatcher<'opts, 'args> {
    fn new(
        pattern: &'args wstr,
        opts: &'opts Match<'args>,
    ) -> Result<StringMatcher<'opts, 'args>, RegexError> {
        if opts.regex {
            let m = RegexMatcher::new(pattern, opts)?;
            Ok(Self::Regex(m))
        } else {
            let m = WildCardMatcher::new(pattern, opts);
            return Ok(Self::WildCard(m));
        }
    }

    fn report_matches(
        &mut self,
        arg: &wstr,
        streams: &mut io_streams_t,
    ) -> Result<(), pcre2::Error> {
        match self {
            Self::Regex(m) => m.report_matches(arg, streams)?,
            Self::WildCard(m) => m.report_matches(arg, streams),
        }
        Ok(())
    }

    fn match_count(&self) -> usize {
        match self {
            Self::Regex(m) => m.total_matched,
            Self::WildCard(m) => m.total_matched,
        }
    }
}

enum MatchResult<'a> {
    NoMatch,
    Match(Option<Captures<'a>>),
}

impl<'opts, 'args> RegexMatcher<'opts, 'args> {
    fn new(
        pattern: &'args wstr,
        opts: &'opts Match<'args>,
    ) -> Result<RegexMatcher<'opts, 'args>, RegexError> {
        let regex = RegexBuilder::new()
            .caseless(opts.ignore_case)
            // UTF-mode can be enabled with `(*UTF)` https://www.pcre.org/current/doc/html/pcre2unicode.html
            // we use the capture group names to set local variables, and those are limited
            // to ascii-alphanumerics and underscores in non-UTF-mode
            // https://www.pcre.org/current/doc/html/pcre2syntax.html#SEC13
            // we can probably relax this limitation as long as we ensure
            // the capture group names are valid variable names
            .never_utf(true)
            .build(pattern.as_char_slice())
            .map_err(|e| RegexError::Compile(pattern.to_owned(), e))?;

        Self::validate_capture_group_names(regex.capture_names())?;

        let first_match_captures = regex
            .capture_names()
            .iter()
            .filter_map(|name| name.as_ref().map(|n| (n.to_owned(), Vec::new())))
            .collect();
        let m = Self {
            regex,
            total_matched: 0,
            first_match_captures,
            opts,
        };
        return Ok(m);
    }

    fn report_matches(
        &mut self,
        arg: &wstr,
        streams: &mut io_streams_t,
    ) -> Result<(), pcre2::Error> {
        let mut iter = self.regex.captures_iter(arg.as_char_slice());
        let cg = iter.next().transpose()?;
        let rc = self.report_match(arg, cg, streams);

        let mut populate_captures = false;
        if let MatchResult::Match(actual) = &rc {
            populate_captures = self.total_matched == 0;
            self.total_matched += 1;

            if populate_captures {
                Self::populate_captures_from_match(
                    &mut self.first_match_captures,
                    self.opts,
                    actual,
                );
            }
        }

        if !self.opts.invert_match && self.opts.all {
            // we are guaranteed to match as long as ops.invert_match is false
            while let MatchResult::Match(cg) =
                self.report_match(arg, iter.next().transpose()?, streams)
            {
                if populate_captures {
                    Self::populate_captures_from_match(
                        &mut self.first_match_captures,
                        self.opts,
                        &cg,
                    );
                }
            }
        }
        Ok(())
    }

    fn populate_captures_from_match<'a>(
        first_match_captures: &mut HashMap<String, Vec<WString>>,
        opts: &Match<'args>,
        cg: &Option<Captures<'a>>,
    ) {
        for (name, captures) in first_match_captures.iter_mut() {
            // If there are multiple named groups and --all was used, we need to ensure that
            // the indexes are always in sync between the variables. If an optional named
            // group didn't match but its brethren did, we need to make sure to put
            // *something* in the resulting array, and unfortunately fish doesn't support
            // empty/null members so we're going to have to use an empty string as the
            // sentinel value.

            if let Some(m) = cg.as_ref().and_then(|cg| cg.name(&name.to_string())) {
                captures.push(WString::from(m.as_bytes()));
            } else if opts.all {
                captures.push(WString::new());
            }
        }
    }

    fn validate_capture_group_names(
        capture_group_names: &[Option<String>],
    ) -> Result<(), RegexError> {
        for name in capture_group_names.iter().filter_map(|n| n.as_ref()) {
            let wname = WString::from_str(name);
            if EnvVar::flags_for(&wname).contains(EnvVarFlags::READ_ONLY) {
                return Err(RegexError::InvalidCaptureGroupName(wname));
            }
        }
        return Ok(());
    }

    fn report_match<'a>(
        &self,
        arg: &'a wstr,
        cg: Option<Captures<'a>>,
        streams: &mut io_streams_t,
    ) -> MatchResult<'a> {
        let Some(cg) = cg else {
            if self.opts.invert_match && !self.opts.quiet {
                if self.opts.index {
                    streams.out.append(sprintf!("1 %lu\n", arg.len()));
                } else {
                    streams.out.appendln(arg);
                }
            }
            return match self.opts.invert_match {
                true => MatchResult::Match(None),
                false => MatchResult::NoMatch,
            };
        };

        if self.opts.invert_match {
            return MatchResult::NoMatch;
        }

        if self.opts.quiet {
            return MatchResult::Match(Some(cg));
        }

        if self.opts.entire {
            streams.out.appendln(arg);
        }

        let start = (self.opts.entire || self.opts.groups_only) as usize;

        for m in (start..cg.len()).filter_map(|i| cg.get(i)) {
            if self.opts.index {
                streams
                    .out
                    .append(sprintf!("%lu %lu\n", m.start() + 1, m.end() - m.start()));
            } else {
                streams.out.appendln(&arg[m.start()..m.end()]);
            }
        }

        return MatchResult::Match(Some(cg));
    }
}

impl<'opts, 'args> WildCardMatcher<'opts, 'args> {
    fn new(pattern: &'args wstr, opts: &'opts Match<'args>) -> Self {
        let mut wcpattern = parse_util_unescape_wildcards(pattern);
        if opts.ignore_case {
            wcpattern = wcpattern.to_lowercase();
        }
        if opts.entire {
            if !wcpattern.is_empty() {
                if wcpattern.char_at(0) != ANY_STRING {
                    wcpattern.insert(0, ANY_STRING);
                }
                if wcpattern.char_at(wcpattern.len() - 1) != ANY_STRING {
                    wcpattern.push(ANY_STRING);
                }
            } else {
                wcpattern.push(ANY_STRING);
            }
        }
        WildCardMatcher {
            pattern: wcpattern,
            total_matched: 0,
            opts,
        }
    }

    fn report_matches(&mut self, arg: &wstr, streams: &mut io_streams_t) {
        // Note: --all is a no-op for glob matching since the pattern is always matched
        // against the entire argument.
        let subject = match self.opts.ignore_case {
            true => arg.to_lowercase(),
            false => arg.to_owned(),
        };
        let m = wildcard_match(subject, &self.pattern, false);

        if m ^ self.opts.invert_match {
            self.total_matched += 1;
            if !self.opts.quiet {
                if self.opts.index {
                    streams.out.append(sprintf!("1 %lu\n", arg.len()));
                } else {
                    streams.out.appendln(arg);
                }
            }
        }
    }
}
