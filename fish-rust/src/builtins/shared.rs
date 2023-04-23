use crate::builtins::{printf, wait};
use crate::ffi::{self, parser_t, wcstring_list_ffi_t, Repin, RustBuiltin};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{c_str, empty_wstring, ToCppWString, WCharFromFFI};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use libc::c_int;
use std::os::fd::RawFd;
use std::pin::Pin;

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

/// Error message when too many arguments are supplied to a builtin.
pub const BUILTIN_ERR_TOO_MANY_ARGUMENTS: &str = "%ls: too many arguments\n";

/// Error message when integer expected
pub const BUILTIN_ERR_NOT_NUMBER: &str = "%ls: %ls: invalid integer\n";

pub const BUILTIN_ERR_MISSING_SUBCMD: &str = "%ls: missing subcommand\n";
pub const BUILTIN_ERR_INVALID_SUBCMD: &str = "%ls: %ls: invalid subcommand\n";

/// Error message for unknown switch.
pub const BUILTIN_ERR_UNKNOWN: &str = "%ls: %ls: unknown option\n";

/// Error messages for unexpected args.
pub const BUILTIN_ERR_ARG_COUNT0: &str = "%ls: missing argument\n";
pub const BUILTIN_ERR_ARG_COUNT1: &str = "%ls: expected %d arguments; got %d\n";
pub const BUILTIN_ERR_ARG_COUNT2: &str = "%ls: %ls: expected %d arguments; got %d\n";
pub const BUILTIN_ERR_MIN_ARG_COUNT1: &str = "%ls: expected >= %d arguments; got %d\n";
pub const BUILTIN_ERR_MAX_ARG_COUNT1: &str = "%ls: expected <= %d arguments; got %d\n";

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

    /// Append a char.
    pub fn append1(&mut self, c: char) -> bool {
        self.append(wstr::from_char_slice(&[c]))
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

fn rust_run_builtin(
    parser: Pin<&mut parser_t>,
    streams: Pin<&mut builtins_ffi::io_streams_t>,
    cpp_args: &wcstring_list_ffi_t,
    builtin: RustBuiltin,
    status_code: &mut i32,
) -> bool {
    let storage: Vec<WString> = cpp_args.from_ffi();
    let mut args: Vec<&wstr> = storage
        .iter()
        .map(|s| match s.chars().position(|c| c == '\0') {
            // We truncate on NUL for backwards-compatibility reasons.
            // This used to be passed as c-strings (`wchar_t *`),
            // and so we act like it for now.
            Some(pos) => &s[..pos],
            None => &s[..],
        })
        .collect();
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
        RustBuiltin::Echo => super::echo::echo(parser, streams, args),
        RustBuiltin::Emit => super::emit::emit(parser, streams, args),
        RustBuiltin::Exit => super::exit::exit(parser, streams, args),
        RustBuiltin::Math => super::math::math(parser, streams, args),
        RustBuiltin::Pwd => super::pwd::pwd(parser, streams, args),
        RustBuiltin::Random => super::random::random(parser, streams, args),
        RustBuiltin::Realpath => super::realpath::realpath(parser, streams, args),
        RustBuiltin::Return => super::r#return::r#return(parser, streams, args),
        RustBuiltin::SetColor => super::set_color::set_color(parser, streams, args),
        RustBuiltin::Status => super::status::status(parser, streams, args),
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
