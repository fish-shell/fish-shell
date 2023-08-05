use std::borrow::Cow;
use std::fs::File;
use std::io::{BufRead, BufReader, Read};
use std::os::fd::FromRawFd;

use crate::common::str2wcstring;
use crate::wcstringutil::fish_wcwidth_visible;
// Forward some imports to make subcmd implementations easier
use crate::{
    builtins::shared::{
        builtin_missing_argument, builtin_print_error_trailer, builtin_print_help, io_streams_t,
        BUILTIN_ERR_ARG_COUNT0, BUILTIN_ERR_ARG_COUNT1, BUILTIN_ERR_COMBO2,
        BUILTIN_ERR_INVALID_SUBCMD, BUILTIN_ERR_MISSING_SUBCMD, BUILTIN_ERR_NOT_NUMBER,
        BUILTIN_ERR_TOO_MANY_ARGUMENTS, BUILTIN_ERR_UNKNOWN, STATUS_CMD_ERROR, STATUS_CMD_OK,
        STATUS_INVALID_ARGS,
    },
    ffi::{parser_t, separation_type_t},
    wchar::{wstr, WString, L},
    wchar_ext::{ToWString, WExt},
    wgetopt::{wgetopter_t, wopt, woption, woption_argument_t::*, NONOPTION_CHAR_CODE},
    wutil::{wgettext, wgettext_fmt},
};
use libc::c_int;

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

fn string_unknown_option(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    subcmd: &wstr,
    opt: &wstr,
) {
    string_error!(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_error_trailer(parser, streams, L!("string"));
}

trait StringSubCommand<'args> {
    const SHORT_OPTIONS: &'static wstr;
    const LONG_OPTIONS: &'static [woption<'static>];

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
        parser: &mut parser_t,
        streams: &mut io_streams_t,
    ) -> Result<usize, Option<c_int>> {
        let cmd = args[0];
        let mut args_read = Vec::with_capacity(args.len());
        args_read.extend_from_slice(args);

        let mut w = wgetopter_t::new(Self::SHORT_OPTIONS, Self::LONG_OPTIONS, args);
        while let Some(c) = w.wgetopt_long() {
            match c {
                ':' => {
                    streams.err.append(L!("string ")); // clone of string_error
                    builtin_missing_argument(parser, streams, cmd, args_read[w.woptind - 1], false);
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    string_unknown_option(parser, streams, cmd, args_read[w.woptind - 1]);
                    return Err(STATUS_INVALID_ARGS);
                }
                c => {
                    let retval = self.parse_opt(cmd, c, w.woptarg);
                    if let Err(e) = retval {
                        e.print_error(&args_read, parser, streams, w.woptarg, w.woptind);
                        return Err(e.retval());
                    }
                }
            }
        }

        return Ok(w.woptind);
    }

    /// Take any positional arguments after options have been parsed.
    #[allow(unused_variables)]
    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'args wstr],
        streams: &mut io_streams_t,
    ) -> Option<c_int> {
        STATUS_CMD_OK
    }

    /// Perform the business logic of the command.
    fn handle(
        &mut self,
        parser: &mut parser_t,
        streams: &mut io_streams_t,
        optind: &mut usize,
        args: &[&'args wstr],
    ) -> Option<c_int>;

    fn run(
        &mut self,
        parser: &mut parser_t,
        streams: &mut io_streams_t,
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

        if streams.stdin_is_directly_redirected() && args.len() > optind {
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
    fn print_error(&self, args: &[&wstr], streams: &mut io_streams_t) {
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
                string_error!(streams, "%ls: %*ls\n", cmd, e.offset().unwrap(), "^");
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
        parser: &mut parser_t,
        streams: &mut io_streams_t,
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

/// A helper type for extracting arguments from either argv or stdin.
struct Arguments<'args, 'iter> {
    /// The list of arguments passed to the string builtin.
    args: &'iter [&'args wstr],
    /// If using argv, index of the next argument to return.
    argidx: &'iter mut usize,
    /// If set, when reading from a stream, split on newlines.
    split_on_newline: bool,
    /// Buffer to store what we read with the BufReader
    /// Is only here to avoid allocating every time
    buffer: Vec<u8>,
    /// If not using argv, we read with a buffer
    reader: Option<BufReader<File>>,
}

impl Drop for Arguments<'_, '_> {
    fn drop(&mut self) {
        if let Some(r) = self.reader.take() {
            // we should not close stdin
            std::mem::forget(r.into_inner());
        }
    }
}

impl<'args, 'iter> Arguments<'args, 'iter> {
    /// Empirically determined.
    /// This is probably down to some pipe buffer or some such,
    /// but too small means we need to call `read(2)` and str2wcstring a lot.
    const STRING_CHUNK_SIZE: usize = 1024;

    fn new(
        args: &'iter [&'args wstr],
        argidx: &'iter mut usize,
        streams: &mut io_streams_t,
    ) -> Self {
        let reader = streams.stdin_is_directly_redirected().then(|| {
            let stdin_fd = streams
                .stdin_fd()
                .filter(|&fd| fd >= 0)
                .expect("should have a valid fd");
            // safety: this should be a valid fd, and already open
            let fd = unsafe { File::from_raw_fd(stdin_fd) };
            BufReader::with_capacity(Self::STRING_CHUNK_SIZE, fd)
        });

        Arguments {
            args,
            argidx,
            split_on_newline: true,
            buffer: Vec::new(),
            reader,
        }
    }

    fn without_splitting_on_newline(
        args: &'iter [&'args wstr],
        argidx: &'iter mut usize,
        streams: &mut io_streams_t,
    ) -> Self {
        let mut args = Self::new(args, argidx, streams);
        args.split_on_newline = false;
        args
    }

    fn get_arg_stdin(&mut self) -> Option<(Cow<'args, wstr>, bool)> {
        let reader = self.reader.as_mut().unwrap();

        // NOTE: C++ wrongly commented that read_blocked retries for EAGAIN
        let num_bytes = match self.split_on_newline {
            true => reader.read_until(b'\n', &mut self.buffer),
            false => reader.read_to_end(&mut self.buffer),
        }
        .ok()?;

        // to match behaviour of earlier versions
        if num_bytes == 0 {
            return None;
        }

        let mut parsed = str2wcstring(&self.buffer);

        // If not set, we have consumed all of stdin and its last line is missing a newline character.
        // This is an edge case -- we expect text input, which is conventionally terminated by a
        // newline character. But if it isn't, we use this to avoid creating one out of thin air,
        // to not corrupt input data.
        let want_newline;
        if self.split_on_newline {
            if parsed.char_at(parsed.len() - 1) == '\n' {
                // consumers do not expect to deal with the newline
                parsed.pop();
                want_newline = true;
            } else {
                // we are missing a trailing newline
                want_newline = false;
            }
        } else {
            want_newline = false;
        }

        let retval = Some((Cow::Owned(parsed), want_newline));
        self.buffer.clear();
        retval
    }
}

impl<'args> Iterator for Arguments<'args, '_> {
    // second is want_newline
    type Item = (Cow<'args, wstr>, bool);

    fn next(&mut self) -> Option<Self::Item> {
        if self.reader.is_some() {
            return self.get_arg_stdin();
        }

        if *self.argidx >= self.args.len() {
            return None;
        }
        *self.argidx += 1;
        return Some((Cow::Borrowed(self.args[*self.argidx - 1]), true));
    }
}

/// The string builtin, for manipulating strings.
pub fn string(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
    let cmd = args[0];
    let argc = args.len();

    if argc <= 1 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_MISSING_SUBCMD, cmd));
        builtin_print_error_trailer(parser, streams, cmd);
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
        "lower" => {
            let mut cmd = transform::Transform {
                quiet: false,
                func: wstr::to_lowercase,
            };
            cmd.run(parser, streams, args)
        }
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
        "upper" => {
            let mut cmd = transform::Transform {
                quiet: false,
                func: wstr::to_uppercase,
            };
            cmd.run(parser, streams, args)
        }
        _ => {
            streams
                .err
                .append(wgettext_fmt!(BUILTIN_ERR_INVALID_SUBCMD, cmd, subcmd_name));
            builtin_print_error_trailer(parser, streams, cmd);
            STATUS_INVALID_ARGS
        }
    }
}
