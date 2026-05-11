use crate::{err_fmt, err_raw, err_str, screen::escape_code_length};
use fish_wcstringutil::fish_wcwidth_visible;
// Forward some imports to make subcmd implementations easier
use super::prelude::*;

mod collect;
mod escape;
mod join;
mod length;
mod r#match;
mod pad;
mod repeat;
mod replace;
mod shorten;
mod split;
mod sub;
mod transform;
mod trim;
mod unescape;

#[cfg(test)]
mod test_helpers;

trait StringSubCommand<'args> {
    const SHORT_OPTIONS: &'static wstr;
    const LONG_OPTIONS: &'static [WOption<'static>];

    /// Parse and store option specified by the associated short or long option.
    fn parse_opt(&mut self, c: char, arg: Option<&'args wstr>) -> Result<(), StringError<'_>>;

    fn parse_opts(
        &mut self,
        args: &mut [&'args wstr],
        parser: &mut Parser,
        streams: &mut IoStreams,
    ) -> Result<usize, ErrorCode> {
        let cmd = L!("string");
        let subcmd = args[0];
        let mut args_read = Vec::with_capacity(args.len());
        args_read.extend_from_slice(args);

        let mut w = WGetopter::new(Self::SHORT_OPTIONS, Self::LONG_OPTIONS, args);
        while let Some(c) = w.next_opt() {
            match c {
                ':' => {
                    builtin_missing_argument(
                        parser,
                        streams,
                        cmd,
                        Some(subcmd),
                        args_read[w.wopt_index - 1],
                        false,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                ';' => {
                    err_fmt!(Error::UNEXP_OPT_ARG, args_read[w.wopt_index - 1])
                        .subcmd(cmd, subcmd)
                        .finish(streams);
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    err_fmt!(Error::UNKNOWN_OPT, args_read[w.wopt_index - 1])
                        .subcmd(cmd, subcmd)
                        .full_trailer(parser)
                        .finish(streams);
                    return Err(STATUS_INVALID_ARGS);
                }
                c => {
                    let retval = self.parse_opt(c, w.woptarg);
                    if let Err(e) = retval {
                        e.print_error(&args_read, streams, w.wopt_index);
                        return Err(STATUS_INVALID_ARGS);
                    }
                }
            }
        }

        Ok(w.wopt_index)
    }

    fn parse_arg_number<'a, 'b>(arg: &'a wstr) -> Result<i64, StringError<'b>> {
        let n = fish_wcstol(arg).map_err(|_| err_fmt!(Error::NOT_NUMBER, arg))?;
        Ok(n)
    }

    /// Take any positional arguments after options have been parsed.
    #[allow(unused_variables)]
    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'args wstr],
        streams: &mut IoStreams,
    ) -> Result<(), ErrorCode> {
        Ok(())
    }

    /// Perform the business logic of the command.
    fn handle(
        &mut self,
        parser: &mut Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&'args wstr],
    ) -> Result<(), ErrorCode>;

    fn run(
        &mut self,
        parser: &mut Parser,
        streams: &mut IoStreams,
        args: &mut [&'args wstr],
    ) -> BuiltinResult {
        match self.run_impl(parser, streams, args) {
            Ok(()) => BuiltinResult::Ok(SUCCESS),
            Err(err) => BuiltinResult::Err(err),
        }
    }

    fn run_impl(
        &mut self,
        parser: &mut Parser,
        streams: &mut IoStreams,
        args: &mut [&'args wstr],
    ) -> Result<(), ErrorCode> {
        if args.len() >= 3 && (args[2] == "-h" || args[2] == "--help") {
            let string_dash_subcmd = WString::from(args[0]) + L!("-") + args[1];
            builtin_print_help(parser, streams, &string_dash_subcmd);
            return Ok(());
        }

        let args = &mut args[1..];

        let mut optind = self.parse_opts(args, parser, streams)?;

        self.take_args(&mut optind, args, streams)?;

        if streams.stdin_is_directly_redirected && args.len() > optind {
            err_str!(Error::TOO_MANY_ARGUMENTS)
                .subcmd(L!("string"), args[0])
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        }

        self.handle(parser, streams, &mut optind, args)
    }
}

/// This covers failing argument/option parsing
enum StringError<'a> {
    InvalidArgs(Error<'a>),
    UnknownOption,
}

enum RegexError {
    Compile {
        pattern: WString,
        error: pcre2::Error,
    },
    InvalidCaptureGroupName {
        name: WString,
    },
    InvalidEscape {
        replacement: WString,
    },
}

impl RegexError {
    fn print_error(&self, args: &[&wstr], streams: &mut IoStreams) {
        let cmd = L!("string");
        let subcmd = args[0];
        use RegexError::*;
        match self {
            Compile { pattern, error } => {
                // TODO: This is misaligned if `pattern` contains characters which are not exactly 1
                // terminal cell wide.
                let mut marker: WString = " "
                    .repeat(error.offset().unwrap_or(0).saturating_sub(1))
                    .into();
                marker.push('^');

                err_fmt!(Error::REGEX_COMPILE, error.error_message())
                    .append_to_msg('\n')
                    .append_to_msg(&err_raw!(pattern).subcmd(cmd, subcmd).to_string())
                    .append_to_msg('\n')
                    .append_to_msg(&err_raw!(marker).subcmd(cmd, subcmd).to_string())
                    .subcmd(cmd, subcmd)
                    .finish(streams);
            }
            InvalidCaptureGroupName { name } => {
                err_fmt!(
                    "Modification of read-only variable \"%s\" is not allowed",
                    name
                )
                .finish(streams);
            }
            InvalidEscape { replacement } => {
                err_fmt!("Invalid escape sequence in pattern \"%s\"", replacement)
                    .subcmd(cmd, subcmd)
                    .finish(streams);
            }
        }
    }
}

impl<'a> From<error::Error<'a>> for StringError<'a> {
    fn from(error: error::Error<'a>) -> Self {
        StringError::InvalidArgs(error)
    }
}

impl<'a> StringError<'a> {
    fn print_error(self, args: &[&wstr], streams: &mut IoStreams, optind: usize) {
        let cmd = L!("string");
        let subcmd = args[0];
        use StringError::*;
        match self {
            InvalidArgs(err) => {
                err.subcmd(cmd, subcmd).finish(streams);
            }
            UnknownOption => {
                // This would mean the subcmd's XXX_OPTIONS does not match
                // the list in its `parse_opt()` implementation
                unreachable!(
                    "unexpected option '{}' for 'string {subcmd}'",
                    args[optind - 1]
                )
            }
        }
    }
}

#[derive(Default, PartialEq, Clone, Copy)]
enum Direction {
    #[default]
    Left,
    Right,
}

fn width_without_escapes(ins: &wstr, start_pos: usize) -> usize {
    let mut width: isize = 0;
    for c in ins[start_pos..].chars() {
        let w = fish_wcwidth_visible(c);
        // We assume that this string is on its own line,
        // in which case a backslash can't bring us below 0.
        if w > 0 || width > 0 {
            width += w;
        }
    }
    // ANSI escape sequences like \e\[31m contain printable characters. Subtract their width
    // because they are not rendered.
    let mut pos = start_pos;
    while let Some(ec_pos) = ins.slice_from(pos).find_char('\x1B') {
        pos += ec_pos;
        if let Some(len) = escape_code_length(ins.slice_from(pos)) {
            let sub = &ins[pos..pos + len];
            for c in sub.chars() {
                width -= fish_wcwidth_visible(c);
            }
            // Move us forward behind the escape code,
            // it might include a second escape!
            // E.g. SGR0 ("reset") is \e\(B\e\[m in xterm.
            pos += len - 1;
        } else {
            pos += 1;
        }
    }
    usize::try_from(width).expect("line has negative width")
}

/// Empirically determined.
/// This is probably down to some pipe buffer or some such,
/// but too small means we need to call `read(2)` and bytes2wcstring a lot.
const STRING_CHUNK_SIZE: usize = 1024;
fn arguments<'iter, 'args>(
    args: &'iter [&'args wstr],
    argidx: &'iter mut usize,
    streams: &mut IoStreams,
) -> Arguments<'args, 'iter> {
    Arguments::new(args, argidx, streams, STRING_CHUNK_SIZE)
}

/// The string builtin, for manipulating strings.
pub fn string(parser: &mut Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let cmd = args[0];
    let argc = args.len();

    if argc <= 1 {
        err_str!(Error::MISSING_SUBCMD)
            .cmd(cmd)
            .full_trailer(parser)
            .finish(streams);
        return Err(STATUS_INVALID_ARGS);
    }

    if args[1] == "-h" || args[1] == "--help" {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    let subcmd_name = args[1];

    match subcmd_name.to_string().as_str() {
        "collect" => collect::Collect::default().run(parser, streams, args),
        "escape" => escape::Escape::default().run(parser, streams, args),
        "join" => join::Join::default().run(parser, streams, args),
        "join0" => {
            let mut cmd = join::Join::default();
            cmd.is_join0 = true;
            cmd.run(parser, streams, args)
        }
        "length" => length::Length::default().run(parser, streams, args),
        "lower" => transform::Transform {
            quiet: false,
            func: wstr::to_lowercase,
        }
        .run(parser, streams, args),
        "match" => r#match::Match::default().run(parser, streams, args),
        "pad" => pad::Pad::default().run(parser, streams, args),
        "repeat" => repeat::Repeat::default().run(parser, streams, args),
        "replace" => replace::Replace::default().run(parser, streams, args),
        "shorten" => shorten::Shorten::default().run(parser, streams, args),
        "split" => split::Split::default().run(parser, streams, args),
        "split0" => {
            let mut cmd = split::Split::default();
            cmd.is_split0 = true;
            cmd.run(parser, streams, args)
        }
        "sub" => sub::Sub::default().run(parser, streams, args),
        "trim" => trim::Trim::default().run(parser, streams, args),
        "unescape" => unescape::Unescape::default().run(parser, streams, args),
        "upper" => transform::Transform {
            quiet: false,
            func: wstr::to_uppercase,
        }
        .run(parser, streams, args),
        _ => {
            err_fmt!(Error::INVALID_SUBCMD)
                .subcmd(cmd, subcmd_name)
                .full_trailer(parser)
                .finish(streams);
            Err(STATUS_INVALID_ARGS)
        }
    }
}
