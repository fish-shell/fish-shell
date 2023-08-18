use crate::builtins::{printf, wait};
use crate::common::str2wcstring;
use crate::ffi::separation_type_t;
use crate::ffi::{self, parser_t, wcstring_list_ffi_t, Repin, RustBuiltin};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{c_str, empty_wstring, ToCppWString, WCharFromFFI};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use cxx::{type_id, ExternType};
use libc::c_int;
use libc::isatty;
use libc::STDOUT_FILENO;

use std::borrow::Cow;
use std::fs::File;
use std::io::{BufRead, BufReader, Read};
use std::os::fd::{FromRawFd, RawFd};
use std::pin::Pin;

pub type BuiltinCmd = fn(&mut parser_t, &mut io_streams_t, &mut [&wstr]) -> Option<c_int>;

#[cxx::bridge]
mod builtins_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("parser.h");
        include!("builtin.h");

        type wcharz_t = crate::ffi::wcharz_t;
        type wcstring_list_ffi_t = crate::ffi::wcstring_list_ffi_t;
        type parser_t = crate::ffi::parser_t;
        type io_streams_t = crate::ffi::io_streams_t;
        type RustBuiltin = crate::ffi::RustBuiltin;
    }
    extern "Rust" {
        fn rust_run_builtin(
            parser: Pin<&mut parser_t>,
            streams: Pin<&mut io_streams_t>,
            cpp_args: &wcstring_list_ffi_t,
            builtin: RustBuiltin,
            status_code: &mut i32,
        ) -> bool;
    }
}

unsafe impl ExternType for io_streams_t {
    type Id = type_id!("io_streams_t");
    type Kind = cxx::kind::Opaque;
}

/// Error message when too many arguments are supplied to a builtin.
pub const BUILTIN_ERR_TOO_MANY_ARGUMENTS: &str = "%ls: too many arguments\n";

/// Error message when integer expected
pub const BUILTIN_ERR_NOT_NUMBER: &str = "%ls: %ls: invalid integer\n";

pub const BUILTIN_ERR_MISSING_SUBCMD: &str = "%ls: missing subcommand\n";
pub const BUILTIN_ERR_INVALID_SUBCMD: &str = "%ls: %ls: invalid subcommand\n";

/// Error message for unknown switch.
pub const BUILTIN_ERR_UNKNOWN: &str = "%ls: %ls: unknown option\n";

pub const BUILTIN_ERR_MISSING_HELP: &str = "fish: %ls: missing man page\nDocumentation may not be installed.\n`help %ls` will show an online version\n";
pub const BUILTIN_ERR_MISSING: &str = "%ls: %ls: option requires an argument\n";

/// Error messages for unexpected args.
pub const BUILTIN_ERR_ARG_COUNT0: &str = "%ls: missing argument\n";
pub const BUILTIN_ERR_ARG_COUNT1: &str = "%ls: expected %d arguments; got %d\n";
pub const BUILTIN_ERR_ARG_COUNT2: &str = "%ls: %ls: expected %d arguments; got %d\n";
pub const BUILTIN_ERR_MIN_ARG_COUNT1: &str = "%ls: expected >= %d arguments; got %d\n";
pub const BUILTIN_ERR_MAX_ARG_COUNT1: &str = "%ls: expected <= %d arguments; got %d\n";

/// Error message for invalid variable name.
pub const BUILTIN_ERR_VARNAME: &str = "%ls: %ls: invalid variable name. See `help identifiers`\n";

/// Error message on invalid combination of options.
pub const BUILTIN_ERR_COMBO: &str = "%ls: invalid option combination\n";
pub const BUILTIN_ERR_COMBO2: &str = "%ls: invalid option combination, %ls\n";
pub const BUILTIN_ERR_COMBO2_EXCLUSIVE: &str = "%ls: %ls %ls: options cannot be used together\n";

// Return values (`$status` values for fish scripts) for various situations.

/// The status code used for normal exit in a command.
pub const STATUS_CMD_OK: Option<c_int> = Some(0);
/// The status code used for failure exit in a command (but not if the args were invalid).
pub const STATUS_CMD_ERROR: Option<c_int> = Some(1);
/// The status code used for invalid arguments given to a command. This is distinct from valid
/// arguments that might result in a command failure. An invalid args condition is something
/// like an unrecognized flag, missing or too many arguments, an invalid integer, etc.
pub const STATUS_INVALID_ARGS: Option<c_int> = Some(2);

/// The status code used when a command was not found.
pub const STATUS_CMD_UNKNOWN: Option<c_int> = Some(127);

/// The status code used when an external command can not be run.
pub const STATUS_NOT_EXECUTABLE: Option<c_int> = Some(126);

/// The status code used when a wildcard had no matches.
pub const STATUS_UNMATCHED_WILDCARD: Option<c_int> = Some(124);
/// The status code used when illegal command name is encountered.
pub const STATUS_ILLEGAL_CMD: Option<c_int> = Some(123);
/// The status code used when `read` is asked to consume too much data.
pub const STATUS_READ_TOO_MUCH: Option<c_int> = Some(122);
/// The status code when an expansion fails, for example, "$foo["
pub const STATUS_EXPAND_ERROR: Option<c_int> = Some(121);

/// A wrapper around output_stream_t.
pub struct output_stream_t(*mut ffi::output_stream_t);

impl output_stream_t {
    /// \return the underlying output_stream_t.
    fn ffi(&mut self) -> Pin<&mut ffi::output_stream_t> {
        unsafe { (*self.0).pin() }
    }

    /// Append a &wstr or WString.
    pub fn append<Str: AsRef<wstr>>(&mut self, s: Str) -> bool {
        self.ffi().append(&s.as_ref().into_cpp())
    }

    /// Append a &wstr or WString with a newline
    pub fn appendln(&mut self, s: impl Into<WString>) -> bool {
        let s = s.into() + L!("\n");
        self.ffi().append(&s.into_cpp())
    }

    /// Append a char.
    pub fn append1(&mut self, c: char) -> bool {
        self.append(wstr::from_char_slice(&[c]))
    }

    pub fn append_with_separation(
        &mut self,
        s: impl AsRef<wstr>,
        sep: separation_type_t,
        want_newline: bool,
    ) -> bool {
        self.ffi()
            .append_with_separation(&s.as_ref().into_cpp(), sep, want_newline)
    }

    pub fn flush_and_check_error(&mut self) -> c_int {
        self.ffi().flush_and_check_error().into()
    }
}

// Convenience wrappers around C++ io_streams_t.
pub struct io_streams_t {
    streams: *mut builtins_ffi::io_streams_t,
    pub out: output_stream_t,
    pub err: output_stream_t,
    pub out_is_redirected: bool,
    pub err_is_redirected: bool,
}

impl io_streams_t {
    pub fn new(mut streams: Pin<&mut builtins_ffi::io_streams_t>) -> io_streams_t {
        let out = output_stream_t(streams.as_mut().get_out().unpin());
        let err = output_stream_t(streams.as_mut().get_err().unpin());
        let out_is_redirected = streams.as_mut().get_out_redirected();
        let err_is_redirected = streams.as_mut().get_err_redirected();
        let streams = streams.unpin();
        io_streams_t {
            streams,
            out,
            err,
            out_is_redirected,
            err_is_redirected,
        }
    }

    pub fn ffi_pin(&mut self) -> Pin<&mut builtins_ffi::io_streams_t> {
        unsafe { Pin::new_unchecked(&mut *self.streams) }
    }

    pub fn ffi_ref(&self) -> &builtins_ffi::io_streams_t {
        unsafe { &*self.streams }
    }

    pub fn out_is_terminal(&self) -> bool {
        !self.out_is_redirected && unsafe { isatty(STDOUT_FILENO) == 1 }
    }

    pub fn stdin_is_directly_redirected(&self) -> bool {
        self.ffi_ref().ffi_stdin_is_directly_redirected()
    }

    pub fn stdin_fd(&self) -> Option<RawFd> {
        let ret = self.ffi_ref().ffi_stdin_fd().0;

        if ret < 0 {
            None
        } else {
            Some(ret)
        }
    }
}

/// Helper function to convert Vec<WString> to Vec<&wstr>, truncating on NUL.
/// We truncate on NUL for backwards-compatibility reasons.
/// This used to be passed as c-strings (`wchar_t *`),
/// and so we act like it for now.
pub fn truncate_args_on_nul(args: &[WString]) -> Vec<&wstr> {
    args.iter()
        .map(|s| match s.chars().position(|c| c == '\0') {
            Some(i) => &s[..i],
            None => &s[..],
        })
        .collect()
}

fn rust_run_builtin(
    parser: Pin<&mut parser_t>,
    streams: Pin<&mut builtins_ffi::io_streams_t>,
    cpp_args: &wcstring_list_ffi_t,
    builtin: RustBuiltin,
    status_code: &mut i32,
) -> bool {
    let storage: Vec<WString> = cpp_args.from_ffi();
    let mut args: Vec<&wstr> = truncate_args_on_nul(&storage);
    let streams = &mut io_streams_t::new(streams);

    match run_builtin(parser.unpin(), streams, args.as_mut_slice(), builtin) {
        None => false,
        Some(status) => {
            *status_code = status;
            true
        }
    }
}

pub fn run_builtin(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
    builtin: RustBuiltin,
) -> Option<c_int> {
    match builtin {
        RustBuiltin::Abbr => super::abbr::abbr(parser, streams, args),
        RustBuiltin::Argparse => super::argparse::argparse(parser, streams, args),
        RustBuiltin::Bg => super::bg::bg(parser, streams, args),
        RustBuiltin::Block => super::block::block(parser, streams, args),
        RustBuiltin::Builtin => super::builtin::builtin(parser, streams, args),
        RustBuiltin::Cd => super::cd::cd(parser, streams, args),
        RustBuiltin::Contains => super::contains::contains(parser, streams, args),
        RustBuiltin::Command => super::command::command(parser, streams, args),
        RustBuiltin::Count => super::count::count(parser, streams, args),
        RustBuiltin::Echo => super::echo::echo(parser, streams, args),
        RustBuiltin::Emit => super::emit::emit(parser, streams, args),
        RustBuiltin::Exit => super::exit::exit(parser, streams, args),
        RustBuiltin::Functions => super::functions::functions(parser, streams, args),
        RustBuiltin::Math => super::math::math(parser, streams, args),
        RustBuiltin::Path => super::path::path(parser, streams, args),
        RustBuiltin::Pwd => super::pwd::pwd(parser, streams, args),
        RustBuiltin::Random => super::random::random(parser, streams, args),
        RustBuiltin::Realpath => super::realpath::realpath(parser, streams, args),
        RustBuiltin::Return => super::r#return::r#return(parser, streams, args),
        RustBuiltin::SetColor => super::set_color::set_color(parser, streams, args),
        RustBuiltin::Status => super::status::status(parser, streams, args),
        RustBuiltin::String => super::string::string(parser, streams, args),
        RustBuiltin::Test => super::test::test(parser, streams, args),
        RustBuiltin::Type => super::r#type::r#type(parser, streams, args),
        RustBuiltin::Wait => wait::wait(parser, streams, args),
        RustBuiltin::Printf => printf::printf(parser, streams, args),
    }
}

// Covers of these functions that take care of the pinning, etc.
// These all return STATUS_INVALID_ARGS.
pub fn builtin_missing_argument(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    cmd: &wstr,
    opt: &wstr,
    print_hints: bool,
) {
    ffi::builtin_missing_argument(
        parser.pin(),
        streams.ffi_pin(),
        c_str!(cmd),
        c_str!(opt),
        print_hints,
    );
}

pub fn builtin_unknown_option(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    cmd: &wstr,
    opt: &wstr,
    print_hints: bool,
) {
    ffi::builtin_unknown_option(
        parser.pin(),
        streams.ffi_pin(),
        c_str!(cmd),
        c_str!(opt),
        print_hints,
    );
}

pub fn builtin_print_help(parser: &mut parser_t, streams: &io_streams_t, cmd: &wstr) {
    ffi::builtin_print_help(
        parser.pin(),
        streams.ffi_ref(),
        c_str!(cmd),
        empty_wstring(),
    );
}

pub fn builtin_print_error_trailer(parser: &mut parser_t, streams: &mut io_streams_t, cmd: &wstr) {
    ffi::builtin_print_error_trailer(parser.pin(), streams.err.ffi(), c_str!(cmd));
}

pub struct HelpOnlyCmdOpts {
    pub print_help: bool,
    pub optind: usize,
}

impl HelpOnlyCmdOpts {
    pub fn parse(
        args: &mut [&wstr],
        parser: &mut parser_t,
        streams: &mut io_streams_t,
    ) -> Result<Self, Option<c_int>> {
        let cmd = args[0];
        let print_hints = true;

        const shortopts: &wstr = L!("+:h");
        const longopts: &[woption] = &[wopt(L!("help"), woption_argument_t::no_argument, 'h')];

        let mut print_help = false;
        let mut w = wgetopter_t::new(shortopts, longopts, args);
        while let Some(c) = w.wgetopt_long() {
            match c {
                'h' => {
                    print_help = true;
                }
                ':' => {
                    builtin_missing_argument(
                        parser,
                        streams,
                        cmd,
                        args[w.woptind - 1],
                        print_hints,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    builtin_unknown_option(parser, streams, cmd, args[w.woptind - 1], print_hints);
                    return Err(STATUS_INVALID_ARGS);
                }
                _ => {
                    panic!("unexpected retval from wgetopter::wgetopt_long()");
                }
            }
        }

        Ok(HelpOnlyCmdOpts {
            print_help,
            optind: w.woptind,
        })
    }
}

#[derive(PartialEq)]
pub enum SplitBehavior {
    Newline,
    /// The default behavior of the -z or --null-in switch,
    /// Automatically start splitting on NULL if one appears in the first PATH_MAX bytes.
    /// Otherwise on newline
    InferNull,
    Null,
    Never,
}

/// A helper type for extracting arguments from either argv or stdin.
pub struct Arguments<'args, 'iter> {
    /// The list of arguments passed to the string builtin.
    args: &'iter [&'args wstr],
    /// If using argv, index of the next argument to return.
    argidx: &'iter mut usize,
    split_behavior: SplitBehavior,
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
    pub fn new(
        args: &'iter [&'args wstr],
        argidx: &'iter mut usize,
        streams: &mut io_streams_t,
        chunk_size: usize,
    ) -> Self {
        let reader = streams.stdin_is_directly_redirected().then(|| {
            let stdin_fd = streams
                .stdin_fd()
                .filter(|&fd| fd >= 0)
                .expect("should have a valid fd");
            // safety: this should be a valid fd, and already open
            let fd = unsafe { File::from_raw_fd(stdin_fd) };
            BufReader::with_capacity(chunk_size, fd)
        });

        Arguments {
            args,
            argidx,
            split_behavior: SplitBehavior::Newline,
            buffer: Vec::new(),
            reader,
        }
    }

    pub fn with_split_behavior(mut self, split_behavior: SplitBehavior) -> Self {
        self.split_behavior = split_behavior;
        self
    }

    fn get_arg_stdin(&mut self) -> Option<(Cow<'args, wstr>, bool)> {
        use SplitBehavior::*;
        let reader = self.reader.as_mut().unwrap();

        if self.split_behavior == InferNull {
            // we must determine if the first `PATH_MAX` bytes contains a null.
            // we intentionally do not consume the buffer here
            // the contents will be returned again later
            let b = reader.fill_buf().ok()?;
            if b.contains(&b'\0') {
                self.split_behavior = Null;
            } else {
                self.split_behavior = Newline;
            }
        }

        // NOTE: C++ wrongly commented that read_blocked retries for EAGAIN
        let num_bytes: usize = match self.split_behavior {
            Newline => reader.read_until(b'\n', &mut self.buffer),
            Null => reader.read_until(b'\0', &mut self.buffer),
            Never => reader.read_to_end(&mut self.buffer),
            _ => unreachable!(),
        }
        .ok()?;

        // to match behaviour of earlier versions
        if num_bytes == 0 {
            return None;
        }

        // assert!(num_bytes == self.buffer.len());
        let (end, want_newline) = match (&self.split_behavior, self.buffer.last().unwrap()) {
            // remove the newline — consumers do not expect it
            (Newline, b'\n') => (num_bytes - 1, true),
            // we are missing a trailing newline!
            (Newline, _) => (num_bytes, false),
            // consumers do not expect to deal with the null
            // "want_newline" is not currently relevant for Null
            (Null, b'\0') => (num_bytes - 1, false),
            // we are missing a null!
            (Null, _) => (num_bytes, false),
            (Never, _) => (num_bytes, false),
            _ => unreachable!(),
        };

        let parsed = str2wcstring(&self.buffer[..end]);

        let retval = Some((Cow::Owned(parsed), want_newline));
        self.buffer.clear();
        retval
    }
}

impl<'args> Iterator for Arguments<'args, '_> {
    // second is want_newline
    // If not set, we have consumed all of stdin and its last line is missing a newline character.
    // This is an edge case -- we expect text input, which is conventionally terminated by a
    // newline character. But if it isn't, we use this to avoid creating one out of thin air,
    // to not corrupt input data.
    type Item = (Cow<'args, wstr>, bool);

    fn next(&mut self) -> Option<Self::Item> {
        if self.reader.is_some() {
            return self.get_arg_stdin();
        }

        if *self.argidx >= self.args.len() {
            return None;
        }
        let retval = (Cow::Borrowed(self.args[*self.argidx]), true);
        *self.argidx += 1;
        return Some(retval);
    }
}
