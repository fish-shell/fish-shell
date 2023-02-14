use crate::builtins::wait;
use crate::ffi::{self, parser_t, wcharz_t, Repin, RustBuiltin};
use crate::wchar::{self, wstr, L};
use crate::wchar_ffi::{c_str, empty_wstring};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use libc::c_int;
use std::pin::Pin;

#[cxx::bridge]
mod builtins_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("parser.h");
        include!("builtin.h");

        type wcharz_t = crate::ffi::wcharz_t;
        type parser_t = crate::ffi::parser_t;
        type io_streams_t = crate::ffi::io_streams_t;
        type RustBuiltin = crate::ffi::RustBuiltin;
    }
    extern "Rust" {
        fn rust_run_builtin(
            parser: Pin<&mut parser_t>,
            streams: Pin<&mut io_streams_t>,
            cpp_args: &Vec<wcharz_t>,
            builtin: RustBuiltin,
            status_code: &mut i32,
        ) -> bool;
    }

    impl Vec<wcharz_t> {}
}

/// A handy return value for successful builtins.
pub const STATUS_CMD_OK: Option<c_int> = Some(0);

/// The status code used for failure exit in a command (but not if the args were invalid).
pub const STATUS_CMD_ERROR: Option<c_int> = Some(1);

/// A handy return value for invalid args.
pub const STATUS_INVALID_ARGS: Option<c_int> = Some(2);

/// Error message when too many arguments are supplied to a builtin.
pub const BUILTIN_ERR_TOO_MANY_ARGUMENTS: &str = "%ls: too many arguments\n";

/// A wrapper around output_stream_t.
pub struct output_stream_t(*mut ffi::output_stream_t);

impl output_stream_t {
    /// \return the underlying output_stream_t.
    fn ffi(&mut self) -> Pin<&mut ffi::output_stream_t> {
        unsafe { (*self.0).pin() }
    }

    /// Append a &wtr or WString.
    pub fn append<Str: AsRef<wstr>>(&mut self, s: Str) -> bool {
        self.ffi().append1(c_str!(s))
    }
}

// Convenience wrappers around C++ io_streams_t.
pub struct io_streams_t {
    streams: *mut builtins_ffi::io_streams_t,
    pub out: output_stream_t,
    pub err: output_stream_t,
}

impl io_streams_t {
    fn new(mut streams: Pin<&mut builtins_ffi::io_streams_t>) -> io_streams_t {
        let out = output_stream_t(streams.as_mut().get_out().unpin());
        let err = output_stream_t(streams.as_mut().get_err().unpin());
        let streams = streams.unpin();
        io_streams_t { streams, out, err }
    }

    fn ffi_pin(&mut self) -> Pin<&mut builtins_ffi::io_streams_t> {
        unsafe { Pin::new_unchecked(&mut *self.streams) }
    }

    fn ffi_ref(&self) -> &builtins_ffi::io_streams_t {
        unsafe { &*self.streams }
    }
}

fn rust_run_builtin(
    parser: Pin<&mut parser_t>,
    streams: Pin<&mut builtins_ffi::io_streams_t>,
    cpp_args: &Vec<wcharz_t>,
    builtin: RustBuiltin,
    status_code: &mut i32,
) -> bool {
    let mut storage = Vec::<wchar::WString>::new();
    for arg in cpp_args {
        storage.push(arg.into());
    }
    let mut args = Vec::new();
    for arg in &storage {
        args.push(arg.as_utfstr());
    }
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
        RustBuiltin::Echo => super::echo::echo(parser, streams, args),
        RustBuiltin::Emit => super::emit::emit(parser, streams, args),
        RustBuiltin::Wait => wait::wait(parser, streams, args),
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
