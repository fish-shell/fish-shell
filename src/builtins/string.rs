use crate::{screen::escape_code_length, wcstringutil::fish_wcwidth_visible};
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

macro_rules! string_error {
    (
    $streams:expr,
    $string:expr
    $(, $args:expr)+
    $(,)?
    ) => {
        $streams.err.append(L!("string "));
        $streams.err.append(wgettext_fmt!($string, $($args),*));
    };
}
use string_error;

fn string_unknown_option(parser: &Parser, streams: &mut IoStreams, subcmd: &wstr, opt: &wstr) {
    string_error!(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_error_trailer(parser, streams.err, L!("string"));
}

trait StringSubCommand<'args> {
    const SHORT_OPTIONS: &'static wstr;
    const LONG_OPTIONS: &'static [WOption<'static>];

    /// Parse and store option specified by the associated short or long option.
    fn parse_opt(
        &mut self,
        name: &wstr,
        c: char,
        arg: Option<&'args wstr>,
    ) -> Result<(), StringError>;

    fn parse_opts(
        &mut self,
        args: &mut [&'args wstr],
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> Result<usize, Option<c_int>> {
        let cmd = args[0];
        let mut args_read = Vec::with_capacity(args.len());
        args_read.extend_from_slice(args);

        let mut w = WGetopter::new(Self::SHORT_OPTIONS, Self::LONG_OPTIONS, args);
        while let Some(c) = w.next_opt() {
            match c {
                ':' => {
                    streams.err.append(L!("string ")); // clone of string_error
                    builtin_missing_argument(
                        parser,
                        streams,
                        cmd,
                        args_read[w.wopt_index - 1],
                        false,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    string_unknown_option(parser, streams, cmd, args_read[w.wopt_index - 1]);
                    return Err(STATUS_INVALID_ARGS);
                }
                c => {
                    let retval = self.parse_opt(cmd, c, w.woptarg);
                    if let Err(e) = retval {
                        e.print_error(&args_read, parser, streams, w.woptarg, w.wopt_index);
                        return Err(e.retval());
                    }
                }
            }
        }

        return Ok(w.wopt_index);
    }

    /// Take any positional arguments after options have been parsed.
    #[allow(unused_variables)]
    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'args wstr],
        streams: &mut IoStreams,
    ) -> Option<c_int> {
        STATUS_CMD_OK
    }

    /// Perform the business logic of the command.
    fn handle(
        &mut self,
        parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&'args wstr],
    ) -> Option<c_int>;

    fn run(
        &mut self,
        parser: &Parser,
        streams: &mut IoStreams,
        args: &mut [&'args wstr],
    ) -> Option<c_int> {
        if args.len() >= 3 && (args[2] == "-h" || args[2] == "--help") {
            let string_dash_subcmd = WString::from(args[0]) + L!("-") + args[1];
            builtin_print_help(parser, streams, &string_dash_subcmd);
            return STATUS_CMD_OK;
        }

        let args = &mut args[1..];

        let mut optind = match self.parse_opts(args, parser, streams) {
            Ok(optind) => optind,
            Err(retval) => return retval,
        };

        let retval = self.take_args(&mut optind, args, streams);
        if retval != STATUS_CMD_OK {
            return retval;
        }

        if streams.stdin_is_directly_redirected && args.len() > optind {
            string_error!(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, args[0]);
            return STATUS_INVALID_ARGS;
        }

        return self.handle(parser, streams, &mut optind, args);
    }
}

/// This covers failing argument/option parsing
enum StringError {
    InvalidArgs(WString),
    NotANumber,
    UnknownOption,
}

enum RegexError {
    Compile(WString, pcre2::Error),
    InvalidCaptureGroupName(WString),
    InvalidEscape(WString),
}

impl RegexError {
    fn print_error(&self, args: &[&wstr], streams: &mut IoStreams) {
        let cmd = args[0];
        use RegexError::*;
        match self {
            Compile(pattern, e) => {
                string_error!(
                    streams,
                    "%ls: Regular expression compile error: %ls\n",
                    cmd,
                    &WString::from(e.error_message())
                );
                string_error!(streams, "%ls: %ls\n", cmd, pattern);
                string_error!(streams, "%ls: %*ls\n", cmd, e.offset().unwrap_or(0), "^");
            }
            InvalidCaptureGroupName(name) => {
                streams.err.append(wgettext_fmt!(
                    "Modification of read-only variable \"%ls\" is not allowed\n",
                    name
                ));
            }
            InvalidEscape(pattern) => {
                string_error!(
                    streams,
                    "%ls: Invalid escape sequence in pattern \"%ls\"\n",
                    cmd,
                    pattern
                );
            }
        }
    }
}

impl From<crate::wutil::wcstoi::Error> for StringError {
    fn from(_: crate::wutil::wcstoi::Error) -> Self {
        StringError::NotANumber
    }
}

macro_rules! invalid_args {
    ($msg:expr, $name:expr, $arg:expr) => {
        StringError::InvalidArgs(crate::wutil::wgettext_fmt!($msg, $name, $arg.unwrap()))
    };
}
use invalid_args;

impl StringError {
    fn print_error(
        &self,
        args: &[&wstr],
        parser: &Parser,
        streams: &mut IoStreams,
        optarg: Option<&wstr>,
        optind: usize,
    ) {
        let cmd = args[0];
        use StringError::*;
        match self {
            InvalidArgs(msg) => {
                streams.err.append(L!("string "));
                // TODO: Once we can extract/edit translations in Rust files, replace this with
                // something like wgettext_fmt!("%ls: %ls\n", cmd, msg) that can be translated
                // and remove the forwarding of the cmd name to `parse_opt`
                streams.err.append(msg);
            }
            NotANumber => {
                string_error!(streams, BUILTIN_ERR_NOT_NUMBER, cmd, optarg.unwrap());
            }
            UnknownOption => {
                string_unknown_option(parser, streams, cmd, args[optind - 1]);
            }
        }
    }

    fn retval(&self) -> Option<c_int> {
        STATUS_INVALID_ARGS
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
/// but too small means we need to call `read(2)` and str2wcstring a lot.
const STRING_CHUNK_SIZE: usize = 1024;
fn arguments<'iter, 'args>(
    args: &'iter [&'args wstr],
    argidx: &'iter mut usize,
    streams: &mut IoStreams,
) -> Arguments<'args, 'iter> {
    Arguments::new(args, argidx, streams, STRING_CHUNK_SIZE)
}

/// The string builtin, for manipulating strings.
pub fn string(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let cmd = args[0];
    let argc = args.len();

    if argc <= 1 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_MISSING_SUBCMD, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if args[1] == "-h" || args[1] == "--help" {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
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
            streams
                .err
                .append(wgettext_fmt!(BUILTIN_ERR_INVALID_SUBCMD, cmd, args[0]));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
    }
}
