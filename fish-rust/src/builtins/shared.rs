use crate::builtins::{printf, wait};
use crate::ffi::{self, wcstring_list_ffi_t, Repin};
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::proc::ProcStatus;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{c_str, empty_wstring, WCharFromFFI};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use libc::c_int;
use std::os::fd::RawFd;
use std::pin::Pin;

/// The default prompt for the read command.
pub const DEFAULT_READ_PROMPT: &str =
    "set_color green; echo -n read; set_color normal; echo -n \"> \"";

/// Error message on missing argument.
pub const BUILTIN_ERR_MISSING: &str = "%ls: %ls: option requires an argument\n";

/// Error message on missing man page.
pub const BUILTIN_ERR_MISSING_HELP: &str = concat!(
    "fish: %ls: missing man page\nDocumentation may not be installed.\n`help %ls` will ",
    "show an online version\n"
);

/// Error message on invalid combination of options.
pub const BUILTIN_ERR_COMBO: &str = "%ls: invalid option combination\n";

/// Error message on invalid combination of options.
pub const BUILTIN_ERR_COMBO2: &str = "%ls: invalid option combination, %ls\n";
pub const BUILTIN_ERR_COMBO2_EXCLUSIVE: &str = "%ls: %ls %ls: options cannot be used together\n";

/// Error message on multiple scope levels for variables.
pub const BUILTIN_ERR_GLOCAL: &str =
    "%ls: scope can be only one of: universal function global local\n";

/// Error message for specifying both export and unexport to set/read.
pub const BUILTIN_ERR_EXPUNEXP: &str = "%ls: cannot both export and unexport\n";

/// Error message for unknown switch.
pub const BUILTIN_ERR_UNKNOWN: &str = "%ls: %ls: unknown option\n";

/// Error message for unexpected args.
pub const BUILTIN_ERR_ARG_COUNT0: &str = "%ls: missing argument\n";
pub const BUILTIN_ERR_ARG_COUNT1: &str = "%ls: expected %d arguments; got %d\n";
pub const BUILTIN_ERR_ARG_COUNT2: &str = "%ls: %ls: expected %d arguments; got %d\n";
pub const BUILTIN_ERR_MIN_ARG_COUNT1: &str = "%ls: expected >= %d arguments; got %d\n";
pub const BUILTIN_ERR_MAX_ARG_COUNT1: &str = "%ls: expected <= %d arguments; got %d\n";

/// Error message for invalid variable name.
pub const BUILTIN_ERR_VARNAME: &str = "%ls: %ls: invalid variable name. See `help identifiers`\n";

/// Error message for invalid bind mode name.
pub const BUILTIN_ERR_BIND_MODE: &str = "%ls: %ls: invalid mode name. See `help identifiers`\n";

/// Error message when too many arguments are supplied to a builtin.
pub const BUILTIN_ERR_TOO_MANY_ARGUMENTS: &str = "%ls: too many arguments\n";

/// Error message when integer expected
pub const BUILTIN_ERR_NOT_NUMBER: &str = "%ls: %ls: invalid integer\n";

/// Command that requires a subcommand was invoked without a recognized subcommand.
pub const BUILTIN_ERR_MISSING_SUBCMD: &str = "%ls: missing subcommand\n";
pub const BUILTIN_ERR_INVALID_SUBCMD: &str = "%ls: %ls: invalid subcommand\n";

/// The send stuff to foreground message.
pub const FG_MSG: &str = "Send job %d (%ls) to foreground\n";

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

pub fn builtin_run<S: AsRef<wstr>>(
    parser: &Parser,
    args: &[S],
    streams: &IoStreams<'_>,
) -> ProcStatus {
    todo!()
}
pub fn builtin_exists(cmd: &wstr) -> bool {
    todo!()
}
// pub fn run_builtin(
//     parser: &mut Parser,
//     streams: &mut IoStreams<'_>,
//     args: &mut [&wstr],
//     builtin: RustBuiltin,
// ) -> Option<c_int> {
//     match builtin {
//         RustBuiltin::Abbr => super::abbr::abbr(parser, streams, args),
//         RustBuiltin::Bg => super::bg::bg(parser, streams, args),
//         RustBuiltin::Block => super::block::block(parser, streams, args),
//         RustBuiltin::Builtin => super::builtin::builtin(parser, streams, args),
//         RustBuiltin::Contains => super::contains::contains(parser, streams, args),
//         RustBuiltin::Command => super::command::command(parser, streams, args),
//         RustBuiltin::Echo => super::echo::echo(parser, streams, args),
//         RustBuiltin::Emit => super::emit::emit(parser, streams, args),
//         RustBuiltin::Exit => super::exit::exit(parser, streams, args),
//         RustBuiltin::Math => super::math::math(parser, streams, args),
//         RustBuiltin::Pwd => super::pwd::pwd(parser, streams, args),
//         RustBuiltin::Random => super::random::random(parser, streams, args),
//         RustBuiltin::Realpath => super::realpath::realpath(parser, streams, args),
//         RustBuiltin::Return => super::r#return::r#return(parser, streams, args),
//         RustBuiltin::Type => super::r#type::r#type(parser, streams, args),
//         RustBuiltin::Wait => wait::wait(parser, streams, args),
//         RustBuiltin::Printf => printf::printf(parser, streams, args),
//     }
// }

pub fn builtin_get_names() -> Vec<&'static wstr> {
    todo!()
}

pub fn builtin_get_desc(name: &wstr) -> WString {
    todo!()
    // let str_ = ffi::builtin_get_desc(&name.to_ffi());
    // if str_.is_null() {
    //     WString::new()
    // } else {
    //     WString::from(&wcharz_t { str_ })
    // }
}

pub fn builtin_print_help(parser: &mut Parser, streams: &IoStreams<'_>, cmd: &wstr) {
    todo!()
    // ffi::builtin_print_help(
    //     parser.
    //     streams.ffi_ref(),
    //     c_str!(cmd),
    //     empty_wstring(),
    // );
}
pub fn builtin_print_help_error(
    parser: &mut Parser,
    streams: &IoStreams<'_>,
    cmd: &wstr,
    error_message: &wstr,
) {
    todo!()
}

// Covers of these functions that take care of the pinning, etc.
// These all return STATUS_INVALID_ARGS.
pub fn builtin_missing_argument(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    cmd: &wstr,
    opt: &wstr,
    print_hints: bool,
) {
    todo!()
    // builtin_missing_argument(
    //     parser.
    //     streams.ffi_pin(),
    //     c_str!(cmd),
    //     c_str!(opt),
    //     print_hints,
    // );
}

pub fn builtin_unknown_option(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    cmd: &wstr,
    opt: &wstr,
    print_hints: bool,
) {
    todo!()
    // ffi::builtin_unknown_option(
    //     parser.
    //     streams.ffi_pin(),
    //     c_str!(cmd),
    //     c_str!(opt),
    //     print_hints,
    // );
}

pub fn builtin_print_error_trailer(parser: &mut Parser, streams: &mut IoStreams<'_>, cmd: &wstr) {
    todo!()
    // ffi::builtin_print_error_trailer(parser. streams.err.ffi(), c_str!(cmd));
}

pub struct HelpOnlyCmdOpts {
    pub print_help: bool,
    pub optind: usize,
}

impl HelpOnlyCmdOpts {
    pub fn parse(
        args: &mut [&wstr],
        parser: &mut Parser,
        streams: &mut IoStreams<'_>,
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
