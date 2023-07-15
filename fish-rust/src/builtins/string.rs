use crate::wcstringutil::fish_wcwidth_visible;
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
        $streams.err.append(&wgettext_fmt!($string, $($args),*));
    };
}
use string_error;

fn string_unknown_option(parser: &Parser, streams: &mut IoStreams<'_>, subcmd: &wstr, opt: &wstr) {
    string_error!(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_error_trailer(parser, streams.err, L!("string"));
}

trait StringSubCommand<'args> {
    fn short_options(&self) -> &'static wstr;
    fn long_options(&self) -> &'static [woption<'static>];

    /// Parse and store option specified by the associated short or long option.
    fn parse_opt(
        &mut self,
        w: &mut wgetopter_t<'static, 'args>,
        c: char,
    ) -> Result<(), StringError>;

    fn parse_opts(
        &mut self,
        w: &mut wgetopter_t<'static, 'args>,
        parser: &Parser,
        streams: &mut IoStreams<'_>,
    ) -> Result<(), Option<c_int>> {
        let mut args_read = Vec::with_capacity(w.argv().len());
        args_read.extend_from_slice(w.argv());

        while let Some(c) = w.wgetopt_long() {
            match c {
                ':' => {
                    streams.err.append(L!("string ")); // clone of string_error
                    builtin_missing_argument(
                        parser,
                        streams,
                        w.cmd(),
                        &args_read[w.woptind - 1],
                        false,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    string_unknown_option(parser, streams, w.cmd(), &args_read[w.woptind - 1]);
                    return Err(STATUS_INVALID_ARGS);
                }
                c => {
                    let retval = self.parse_opt(w, c);
                    if let Err(e) = retval {
                        e.print_error(&args_read, parser, streams, w.woptarg(), w.woptind);
                        return Err(e.retval());
                    }
                }
            }
        }

        Ok(())
    }

    /// Take any positional arguments after options have been parsed.
    #[allow(unused_variables)]
    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &'args [WString],
        streams: &mut IoStreams<'_>,
    ) -> Option<c_int> {
        STATUS_CMD_OK
    }

    /// Perform the business logic of the command.
    fn handle(
        &mut self,
        parser: &Parser,
        streams: &mut IoStreams<'_>,
        optind: &mut usize,
        args: &'args [WString],
    ) -> Option<c_int>;

    fn run(
        &mut self,
        parser: &Parser,
        streams: &mut IoStreams<'_>,
        subcmd_name: &'args wstr,
        w: &'args mut wgetopter_t<'static, 'args>,
    ) -> Option<c_int> {
        if w.argv().len() >= 2 && (w.argv()[1] == "-h" || w.argv()[1] == "--help") {
            let string_dash_subcmd = WString::from(subcmd_name) + L!("-") + &w.argv()[0][..];
            builtin_print_help(parser, streams, &string_dash_subcmd);
            return STATUS_CMD_OK;
        }

        match self.parse_opts(w, parser, streams) {
            Ok(()) => (),
            Err(retval) => return retval,
        };

        let mut optind = w.woptind;
        let retval = self.take_args(&mut optind, w.argv(), streams);
        if retval != STATUS_CMD_OK {
            return retval;
        }

        if streams.stdin_is_directly_redirected && w.argv().len() > optind {
            string_error!(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, w.argv()[0]);
            return STATUS_INVALID_ARGS;
        }

        return self.handle(parser, streams, &mut optind, w.argv());
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
    fn print_error(&self, args: &[WString], streams: &mut IoStreams<'_>) {
        let cmd = &args[0];
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
                string_error!(streams, "%ls: %*ls\n", cmd, e.offset().unwrap(), "^");
            }
            InvalidCaptureGroupName(name) => {
                streams.err.append(&wgettext_fmt!(
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
        args: &[WString],
        parser: &Parser,
        streams: &mut IoStreams<'_>,
        optarg: Option<&wstr>,
        optind: usize,
    ) {
        let cmd = &args[0];
        use StringError::*;
        match self {
            InvalidArgs(msg) => {
                streams.err.append(L!("string "));
                // TODO: Once we can extract/edit translations in Rust files, replace this with
                // something like wgettext_fmt!("%ls: %ls\n", cmd, msg) that can be translated
                // and remove the forwarding of the cmd name to `parse_opt`
                streams.err.append(&msg);
            }
            NotANumber => {
                string_error!(streams, BUILTIN_ERR_NOT_NUMBER, cmd, optarg.unwrap());
            }
            UnknownOption => {
                string_unknown_option(parser, streams, cmd, &args[optind - 1]);
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
    let mut width: i32 = 0;
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
    // we subtracted less than we added
    debug_assert!(width >= 0, "line has negative width");
    return width as usize;
}

fn escape_code_length(code: &wstr) -> Option<usize> {
    use crate::ffi::escape_code_length_ffi;
    use crate::wchar_ffi::wstr_to_u32string;

    match escape_code_length_ffi(wstr_to_u32string(code).as_ptr()).into() {
        -1 => None,
        n => Some(n as usize),
    }
}

/// Empirically determined.
/// This is probably down to some pipe buffer or some such,
/// but too small means we need to call `read(2)` and str2wcstring a lot.
const STRING_CHUNK_SIZE: usize = 1024;
fn arguments<'iter>(
    args: &'iter [WString],
    argidx: &'iter mut usize,
    streams: &mut IoStreams<'_>,
) -> Arguments<'iter> {
    Arguments::new(args, argidx, streams, STRING_CHUNK_SIZE)
}

/// The string builtin, for manipulating strings.
pub fn string(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    let argc = args.len();

    if argc <= 1 {
        let cmd = &args[0];
        streams
            .err
            .append(&wgettext_fmt!(BUILTIN_ERR_MISSING_SUBCMD, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if args[1] == "-h" || args[1] == "--help" {
        let cmd = &args[0];
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    let (cmd_names, args) = args.split_at_mut(2);
    let cmd = &cmd_names[0];
    let subcmd_name = &cmd_names[1];

    let mut subcmd: Box<dyn StringSubCommand<'_>> = match subcmd_name.to_string().as_str() {
        "collect" => Box::new(collect::Collect::default()),
        "escape" => Box::new(escape::Escape::default()),
        "join" => Box::new(join::Join::default()),
        "join0" => {
            let mut cmd = Box::new(join::Join::default());
            cmd.is_join0 = true;
            cmd
        }
        "length" => Box::new(length::Length::default()),
        "lower" => Box::new(transform::Transform {
            quiet: false,
            func: wstr::to_lowercase,
        }),
        "match" => Box::new(r#match::Match::default()),
        "pad" => Box::new(pad::Pad::default()),
        "repeat" => Box::new(repeat::Repeat::default()),
        "replace" => Box::new(replace::Replace::default()),
        "shorten" => Box::new(shorten::Shorten::default()),
        "split" => Box::new(split::Split::default()),
        "split0" => {
            let mut cmd = Box::new(split::Split::default());
            cmd.is_split0 = true;
            cmd
        }
        "sub" => Box::new(sub::Sub::default()),
        "trim" => Box::new(trim::Trim::default()),
        "unescape" => Box::new(unescape::Unescape::default()),
        "upper" => Box::new(transform::Transform {
            quiet: false,
            func: wstr::to_uppercase,
        }),
        _ => {
            streams
                .err
                .append(&wgettext_fmt!(BUILTIN_ERR_INVALID_SUBCMD, cmd, subcmd_name));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
    };
    let mut w = wgetopter_t::new(subcmd.short_options(), subcmd.long_options(), args);
    let result = subcmd.run(parser, streams, subcmd_name, &mut w);
    drop(subcmd);
    result
}
