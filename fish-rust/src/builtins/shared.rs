use crate::builtins::{printf, wait};
use crate::common::{escape, get_by_sorted_name, read_blocked, str2wcstring, Named};
use crate::ffi::{self, wcstring_list_ffi_t, Repin};
use crate::flog::FLOGF;
use crate::io::{IoChain, IoFd, IoStreams, OutputStream};
use crate::null_terminated_array::OwningNullTerminatedArray;
use crate::parse_constants::UNKNOWN_BUILTIN_ERR_MSG;
use crate::parse_util::parse_util_argument_is_help;
use crate::parser::{Block, BlockType, LoopStatus, Parser};
use crate::proc::{no_exec, ProcStatus};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{c_str, empty_wstring, W0String, WCharFromFFI, WCharToFFI};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::{perror, sprintf, wgettext, wgettext_fmt};
use cxx::{CxxWString, UniquePtr};
use errno::errno;
use libc::{c_int, STDERR_FILENO, STDIN_FILENO, STDOUT_FILENO};
use std::ffi::{CStr, CString};
use std::os::fd::RawFd;
use std::pin::Pin;
use std::sync::Arc;
use widestring_suffix::widestrs;

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

/// Data structure to describe a builtin.
struct BuiltinData {
    // Name of the builtin.
    name: &'static wstr,
    // Function pointer to the builtin implementation.
    func: BuiltinImplementation,
}

impl BuiltinData {
    const fn new(name: &'static wstr, func: BuiltinImplementation) -> Self {
        Self { name, func }
    }
}

enum BuiltinImplementation {
    Rust(
        fn(parser: &mut Parser, streams: &mut IoStreams<'_>, argv: &mut [WString]) -> Option<c_int>,
    ),
    Cpp,
}

// Data about all the builtin commands in fish.
// Functions that are bound to builtin_generic are handled directly by the parser.
// NOTE: These must be kept in sorted order!
#[widestrs]
const BUILTIN_DATAS: &[BuiltinData] = &[
    BuiltinData::new("."L, BuiltinImplementation::Cpp),
    BuiltinData::new(":"L, BuiltinImplementation::Rust(builtin_true)),
    BuiltinData::new(
        "["L, // ]
        BuiltinImplementation::Cpp,
    ),
    BuiltinData::new("_"L, BuiltinImplementation::Rust(builtin_gettext)),
    BuiltinData::new("abbr"L, BuiltinImplementation::Cpp),
    BuiltinData::new("and"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("argparse"L, BuiltinImplementation::Cpp),
    BuiltinData::new("begin"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("bg"L, BuiltinImplementation::Cpp),
    BuiltinData::new("bind"L, BuiltinImplementation::Cpp),
    BuiltinData::new("block"L, BuiltinImplementation::Cpp),
    BuiltinData::new(
        "break"L,
        BuiltinImplementation::Rust(builtin_break_continue),
    ),
    BuiltinData::new(
        "breakpoint"L,
        BuiltinImplementation::Rust(builtin_breakpoint),
    ),
    BuiltinData::new("builtin"L, BuiltinImplementation::Cpp),
    BuiltinData::new("case"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("cd"L, BuiltinImplementation::Cpp),
    BuiltinData::new("command"L, BuiltinImplementation::Cpp),
    BuiltinData::new("commandline"L, BuiltinImplementation::Cpp),
    BuiltinData::new("complete"L, BuiltinImplementation::Cpp),
    BuiltinData::new("contains"L, BuiltinImplementation::Cpp),
    BuiltinData::new(
        "continue"L,
        BuiltinImplementation::Rust(builtin_break_continue),
    ),
    BuiltinData::new("count"L, BuiltinImplementation::Rust(builtin_count)),
    BuiltinData::new("disown"L, BuiltinImplementation::Cpp),
    BuiltinData::new("echo"L, BuiltinImplementation::Cpp),
    BuiltinData::new("else"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("emit"L, BuiltinImplementation::Cpp),
    BuiltinData::new("end"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("eval"L, BuiltinImplementation::Cpp),
    BuiltinData::new("exec"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("exit"L, BuiltinImplementation::Cpp),
    BuiltinData::new("false"L, BuiltinImplementation::Rust(builtin_false)),
    BuiltinData::new("fg"L, BuiltinImplementation::Cpp),
    BuiltinData::new("for"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("function"L, BuiltinImplementation::Cpp),
    BuiltinData::new("functions"L, BuiltinImplementation::Cpp),
    BuiltinData::new("history"L, BuiltinImplementation::Cpp),
    BuiltinData::new("if"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("jobs"L, BuiltinImplementation::Cpp),
    BuiltinData::new("math"L, BuiltinImplementation::Cpp),
    BuiltinData::new("not"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("or"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("path"L, BuiltinImplementation::Cpp),
    BuiltinData::new("printf"L, BuiltinImplementation::Cpp),
    BuiltinData::new("pwd"L, BuiltinImplementation::Cpp),
    BuiltinData::new("random"L, BuiltinImplementation::Cpp),
    BuiltinData::new("read"L, BuiltinImplementation::Cpp),
    BuiltinData::new("realpath"L, BuiltinImplementation::Cpp),
    BuiltinData::new("return"L, BuiltinImplementation::Cpp),
    BuiltinData::new("set"L, BuiltinImplementation::Cpp),
    BuiltinData::new("set_color"L, BuiltinImplementation::Cpp),
    BuiltinData::new("source"L, BuiltinImplementation::Cpp),
    BuiltinData::new("status"L, BuiltinImplementation::Cpp),
    BuiltinData::new("string"L, BuiltinImplementation::Cpp),
    BuiltinData::new("switch"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("test"L, BuiltinImplementation::Cpp),
    BuiltinData::new("time"L, BuiltinImplementation::Rust(builtin_generic)),
    BuiltinData::new("true"L, BuiltinImplementation::Rust(builtin_true)),
    BuiltinData::new("type"L, BuiltinImplementation::Cpp),
    BuiltinData::new("ulimit"L, BuiltinImplementation::Cpp),
    BuiltinData::new("wait"L, BuiltinImplementation::Cpp),
    BuiltinData::new("while"L, BuiltinImplementation::Rust(builtin_generic)),
];
assert_sorted_by_name!(BUILTIN_DATAS);

impl Named for BuiltinData {
    fn name(&self) -> &'static wstr {
        self.name
    }
}

/// Look up a builtin_data_t for a specified builtin
///
/// @param  name
///    Name of the builtin
///
/// @return
///    Pointer to a builtin_data_t
///
fn builtin_lookup(name: &wstr) -> Option<&'static BuiltinData> {
    get_by_sorted_name(name, BUILTIN_DATAS)
}

/// Is there a builtin command with the given name?
pub fn builtin_exists(name: &wstr) -> bool {
    builtin_lookup(name).is_some()
}

/// Is the command a keyword we need to special-case the handling of `-h` and `--help`.
#[widestrs]
fn cmd_needs_help(cmd: &wstr) -> bool {
    [
        "for"L,
        "while"L,
        "function"L,
        "if"L,
        "end"L,
        "switch"L,
        "case"L,
    ]
    .contains(&cmd)
}

#[widestrs]
fn builtin_run_cpp(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &[WString],
) -> Option<c_int> {
    todo!()
    // let cmdname = argv[0];
    // let parser = parser.pin();
    // let streams = streams.pin();
    // let parser = &mut parser as *mut _ as *mut autocxx::c_void;
    // let streams = &mut streams as *mut _ as *mut autocxx::c_void;
    // let mut argv: Vec<*const u32> = argv.iter().map(|s| c_str!(s)).collect();
    // argv.push(std::ptr::null());
    // let argv = std::ptr::addr_of!(argv[0]) as *const autocxx::c_void;
    // let ret = match cmdname {
    //     _ if cmdname == "."L => ffi::builtin_source_ffi(parser, streams, argv),
    //     _ if cmdname == "["L => ffi::builtin_test_ffi(parser, streams, argv),
    //     _ if cmdname == "argparse"L => ffi::builtin_argparse_ffi(parser, streams, argv),
    //     _ if cmdname == "bind"L => ffi::builtin_bind_ffi(parser, streams, argv),
    //     _ if cmdname == "cd"L => ffi::builtin_cd_ffi(parser, streams, argv),
    //     _ if cmdname == "commandline"L => ffi::builtin_commandline_ffi(parser, streams, argv),
    //     _ if cmdname == "complete"L => ffi::builtin_complete_ffi(parser, streams, argv),
    //     _ if cmdname == "disown"L => ffi::builtin_disown_ffi(parser, streams, argv),
    //     _ if cmdname == "eval"L => ffi::builtin_eval_ffi(parser, streams, argv),
    //     _ if cmdname == "fg"L => ffi::builtin_fg_ffi(parser, streams, argv),
    //     _ if cmdname == "function"L => ffi::builtin_function_ffi(parser, streams, argv),
    //     _ if cmdname == "functions"L => ffi::builtin_functions_ffi(parser, streams, argv),
    //     _ if cmdname == "history"L => ffi::builtin_history_ffi(parser, streams, argv),
    //     _ if cmdname == "jobs"L => ffi::builtin_jobs_ffi(parser, streams, argv),
    //     _ if cmdname == "path"L => ffi::builtin_path_ffi(parser, streams, argv),
    //     _ if cmdname == "read"L => ffi::builtin_read_ffi(parser, streams, argv),
    //     _ if cmdname == "set"L => ffi::builtin_set_ffi(parser, streams, argv),
    //     _ if cmdname == "set_color"L => ffi::builtin_set_color_ffi(parser, streams, argv),
    //     _ if cmdname == "source"L => ffi::builtin_source_ffi(parser, streams, argv),
    //     _ if cmdname == "status"L => ffi::builtin_status_ffi(parser, streams, argv),
    //     _ if cmdname == "string"L => ffi::builtin_string_ffi(parser, streams, argv),
    //     _ if cmdname == "test"L => ffi::builtin_test_ffi(parser, streams, argv),
    //     _ if cmdname == "ulimit"L => ffi::builtin_ulimit_ffi(parser, streams, argv),
    //     _ => panic!(),
    // };
    // if ret.0 == c_int::MAX {
    //     None
    // } else {
    //     Some(ret.0)
    // }
}

/// Execute a builtin command
pub fn builtin_run(
    parser: &mut Parser,
    argv: &mut [WString],
    streams: &mut IoStreams<'_>,
) -> ProcStatus {
    if argv.is_empty() {
        return ProcStatus::from_exit_code(STATUS_INVALID_ARGS.unwrap());
    }

    // We can be handed a keyword by the parser as if it was a command. This happens when the user
    // follows the keyword by `-h` or `--help`. Since it isn't really a builtin command we need to
    // handle displaying help for it here.
    if argv.len() == 2 && parse_util_argument_is_help(&argv[1]) && cmd_needs_help(&argv[0]) {
        builtin_print_help(parser, streams, &argv[0]);
        return ProcStatus::from_exit_code(STATUS_CMD_OK.unwrap());
    }

    let Some(builtin) = builtin_lookup(&argv[0]) else {
        FLOGF!(error, "%s", wgettext_fmt!(UNKNOWN_BUILTIN_ERR_MSG, &argv[0]));
        return ProcStatus::from_exit_code(STATUS_CMD_ERROR.unwrap());
    };

    let builtin_ret = match &builtin.func {
        BuiltinImplementation::Cpp => {
            // Construct the permutable argv array which the builtin expects, and execute the builtin.
            builtin_run_cpp(parser, streams, argv)
        }
        BuiltinImplementation::Rust(func) => (func)(parser, streams, argv),
    };

    // Flush our out and error streams, and check for their errors.
    let out_ret = streams.out.flush_and_check_error();
    let err_ret = streams.err.flush_and_check_error();

    // Resolve our status code.
    // If the builtin itself produced an error, use that error.
    // Otherwise use any errors from writing to out and writing to err, in that order.
    let mut code = builtin_ret.unwrap_or(0);
    if code == 0 {
        code = out_ret;
    }
    if code == 0 {
        code = err_ret;
    }

    // The exit code is cast to an 8-bit unsigned integer, so saturate to 255. Otherwise,
    // multiples of 256 are reported as 0.
    if code > 255 {
        code = 255;
    }

    // Handle the case of an empty status.
    if code == 0 && builtin_ret.is_none() {
        return ProcStatus::empty();
    }
    if code < 0 {
        // If the code is below 0, constructing a proc_status_t
        // would assert() out, which is a terrible failure mode
        // So instead, what we do is we get a positive code,
        // and we avoid 0.
        code = ((256 + code) % 256).abs();
        if code == 0 {
            code = 255;
        }
        FLOGF!(
            warning,
            "builtin %ls returned invalid exit code %d",
            &argv[0],
            code
        );
    }

    ProcStatus::from_exit_code(code)
}

/// Returns a list of all builtin names.
pub fn builtin_get_names() -> impl Iterator<Item = &'static wstr> {
    BUILTIN_DATAS.iter().map(|builtin| builtin.name)
}

/// Return a one-line description of the specified builtin.
#[widestrs]
pub fn builtin_get_desc(name: &wstr) -> Option<&'static wstr> {
    let desc = match name {
        _ if name == "."L => wgettext!("Evaluate contents of file"),
        _ if name == ":"L => wgettext!("Return a successful result"),
        _ if name == "["L => wgettext!("Test a condition"), // ]
        _ if name == "_"L => wgettext!("Translate a string"),
        _ if name == "abbr"L => wgettext!("Manage abbreviations"),
        _ if name == "and"L => wgettext!("Run command if last command succeeded"),
        _ if name == "argparse"L => wgettext!("Parse options in fish script"),
        _ if name == "begin"L => wgettext!("Create a block of code"),
        _ if name == "bg"L => wgettext!("Send job to background"),
        _ if name == "bind"L => wgettext!("Handle fish key bindings"),
        _ if name == "block"L => wgettext!("Temporarily block delivery of events"),
        _ if name == "break"L => wgettext!("Stop the innermost loop"),
        _ if name == "breakpoint"L => wgettext!("Halt execution and start debug prompt"),
        _ if name == "builtin"L => wgettext!("Run a builtin specifically"),
        _ if name == "case"L => wgettext!("Block of code to run conditionally"),
        _ if name == "cd"L => wgettext!("Change working directory"),
        _ if name == "command"L => wgettext!("Run a command specifically"),
        _ if name == "commandline"L => wgettext!("Set or get the commandline"),
        _ if name == "complete"L => wgettext!("Edit command specific completions"),
        _ if name == "contains"L => wgettext!("Search for a specified string in a list"),
        _ if name == "continue"L => wgettext!("Skip over remaining innermost loop"),
        _ if name == "count"L => wgettext!("Count the number of arguments"),
        _ if name == "disown"L => wgettext!("Remove job from job list"),
        _ if name == "echo"L => wgettext!("Print arguments"),
        _ if name == "else"L => wgettext!("Evaluate block if condition is false"),
        _ if name == "emit"L => wgettext!("Emit an event"),
        _ if name == "end"L => wgettext!("End a block of commands"),
        _ if name == "eval"L => wgettext!("Evaluate a string as a statement"),
        _ if name == "exec"L => wgettext!("Run command in current process"),
        _ if name == "exit"L => wgettext!("Exit the shell"),
        _ if name == "false"L => wgettext!("Return an unsuccessful result"),
        _ if name == "fg"L => wgettext!("Send job to foreground"),
        _ if name == "for"L => wgettext!("Perform a set of commands multiple times"),
        _ if name == "function"L => wgettext!("Define a new function"),
        _ if name == "functions"L => wgettext!("List or remove functions"),
        _ if name == "history"L => wgettext!("History of commands executed by user"),
        _ if name == "if"L => wgettext!("Evaluate block if condition is true"),
        _ if name == "jobs"L => wgettext!("Print currently running jobs"),
        _ if name == "math"L => wgettext!("Evaluate math expressions"),
        _ if name == "not"L => wgettext!("Negate exit status of job"),
        _ if name == "or"L => wgettext!("Execute command if previous command failed"),
        _ if name == "path"L => wgettext!("Handle paths"),
        _ if name == "printf"L => wgettext!("Prints formatted text"),
        _ if name == "pwd"L => wgettext!("Print the working directory"),
        _ if name == "random"L => wgettext!("Generate random number"),
        _ if name == "read"L => wgettext!("Read a line of input into variables"),
        _ if name == "realpath"L => wgettext!("Show absolute path sans symlinks"),
        _ if name == "return"L => wgettext!("Stop the currently evaluated function"),
        _ if name == "set"L => wgettext!("Handle environment variables"),
        _ if name == "set_color"L => wgettext!("Set the terminal color"),
        _ if name == "source"L => wgettext!("Evaluate contents of file"),
        _ if name == "status"L => wgettext!("Return status information about fish"),
        _ if name == "string"L => wgettext!("Manipulate strings"),
        _ if name == "switch"L => wgettext!("Conditionally run blocks of code"),
        _ if name == "test"L => wgettext!("Test a condition"),
        _ if name == "time"L => wgettext!("Measure how long a command or block takes"),
        _ if name == "true"L => wgettext!("Return a successful result"),
        _ if name == "type"L => wgettext!("Check if a thing is a thing"),
        _ if name == "ulimit"L => wgettext!("Get/set resource usage limits"),
        _ if name == "wait"L => wgettext!("Wait for background processes completed"),
        _ if name == "while"L => wgettext!("Perform a command multiple times"),
        _ => return None,
    };
    Some(desc)
}

/// Display help/usage information for the specified builtin or function from manpage
///
/// @param  name
///    builtin or function name to get up help for
///
/// Process and print help for the specified builtin or function.
pub fn builtin_print_help(parser: &mut Parser, streams: &mut IoStreams<'_>, cmd: &wstr) {
    builtin_print_help_error(parser, streams, cmd, L!(""))
}
pub fn builtin_print_help_error(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    cmd: &wstr,
    error_message: &wstr,
) {
    // This won't ever work if no_exec is set.
    if no_exec() {
        return;
    }
    let name_esc = escape(cmd);
    let mut cmd = sprintf!("__fish_print_help %ls ", &name_esc);
    let mut ios = IoChain::new();
    if !error_message.is_empty() {
        cmd.push_utfstr(&escape(error_message));
        // If it's an error, redirect the output of __fish_print_help to stderr
        ios.push(Arc::new(IoFd::new(STDOUT_FILENO, STDERR_FILENO)));
    }
    let res = parser.eval(&cmd, &ios);
    if res.status.normal_exited() && res.status.exit_code() == 2 {
        streams
            .err
            .append(&wgettext_fmt!(BUILTIN_ERR_MISSING_HELP, name_esc, name_esc));
    }
}

/// Perform error reporting for encounter with unknown option.
pub fn builtin_unknown_option(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    cmd: &wstr,
    opt: &wstr,
    print_hints: bool,
) {
    streams
        .err
        .append(&wgettext_fmt!(BUILTIN_ERR_UNKNOWN, cmd, opt));
    if print_hints {
        builtin_print_error_trailer(parser, streams.err, cmd);
    }
}

/// Perform error reporting for encounter with missing argument.
pub fn builtin_missing_argument(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    cmd: &wstr,
    mut opt: &wstr,
    print_hints: bool,
) {
    if opt.chars().take(2).any(|c| c == '-') {
        // if c in -qc '-qc' is missing the argument, now opt is just 'c'
        opt = &opt[opt.len() - 1..];
        streams.err.append(&wgettext_fmt!(
            BUILTIN_ERR_MISSING,
            cmd,
            L!("-").to_owned() + opt
        ));
    } else {
        streams
            .err
            .append(&wgettext_fmt!(BUILTIN_ERR_MISSING, cmd, opt));
    }
    if print_hints {
        builtin_print_error_trailer(parser, streams.err, cmd);
    }
}

/// Print the backtrace and call for help that we use at the end of error messages.
pub fn builtin_print_error_trailer(parser: &mut Parser, b: &mut dyn OutputStream, cmd: &wstr) {
    b.push('\n');
    let stacktrace = parser.current_line();
    // Don't print two empty lines if we don't have a stacktrace.
    if !stacktrace.is_empty() {
        b.append(&stacktrace);
        b.push('\n');
    }
    b.append(&wgettext_fmt!(
        "(Type 'help %ls' for related documentation)\n",
        cmd
    ));
}

/// This function works like perror, but it prints its result into the streams.err string instead
/// to stderr. Used by the builtin commands.
pub fn builtin_wperror(program_name: &wstr, streams: &mut IoStreams<'_>) {
    let err = errno();
    streams.err.append(program_name);
    streams.err.append(L!(": "));
    if err.0 != 0 {
        let werr = WString::from_str(&err.to_string());
        streams.err.append(&werr);
        streams.err.push('\n');
    }
}

pub struct HelpOnlyCmdOpts {
    pub print_help: bool,
    pub optind: usize,
}

impl HelpOnlyCmdOpts {
    pub fn parse(
        args: &mut [WString],
        parser: &mut Parser,
        streams: &mut IoStreams<'_>,
    ) -> Result<Self, Option<c_int>> {
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
                        w.cmd(),
                        &w.argv()[w.woptind - 1],
                        print_hints,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    builtin_unknown_option(
                        parser,
                        streams,
                        w.cmd(),
                        &w.argv()[w.woptind - 1],
                        print_hints,
                    );
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

/// A generic builtin that only supports showing a help message. This is only a placeholder that
/// prints the help message. Useful for commands that live in the parser.
fn builtin_generic(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let argc = argv.len();
    let opts = match HelpOnlyCmdOpts::parse(argv, parser, streams) {
        Ok(opts) => opts,
        Err(err) => return err,
    };

    if opts.print_help {
        builtin_print_help(parser, streams, &argv[0]);
        return STATUS_CMD_OK;
    }

    // Hackish - if we have no arguments other than the command, we are a "naked invocation" and we
    // just print help.
    if argc == 1 || &argv[0] == L!("time") {
        builtin_print_help(parser, streams, &argv[0]);
        return STATUS_INVALID_ARGS;
    }

    STATUS_CMD_ERROR
}

// How many bytes we read() at once.
// Since this is just for counting, it can be massive.
const COUNT_CHUNK_SIZE: usize = 512 * 256;
/// Implementation of the builtin count command, used to count the number of arguments sent to it.
fn builtin_count(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let mut argc = 0;

    // Count the newlines coming in via stdin like `wc -l`.
    if streams.stdin_is_directly_redirected {
        assert!(
            streams.stdin_fd >= 0,
            "Should have a valid fd since stdin is directly redirected"
        );
        let mut buf = [b'\0'; COUNT_CHUNK_SIZE];
        loop {
            let n = read_blocked(streams.stdin_fd, &mut buf);
            if n == 0 {
                break;
            } else if n < 0 {
                perror("read");
                return STATUS_CMD_ERROR;
            }
            buf[..n as usize].iter().filter(|c| **c == b'\n').count();
        }
    }

    // Always add the size of argv.
    // That means if you call `something | count a b c`, you'll get the count of something _plus 3_.
    argc += argv.len() - 1;
    streams.out.append(&sprintf!("%d\n", argc));
    if argc == 0 {
        STATUS_CMD_ERROR
    } else {
        STATUS_CMD_OK
    }
}

/// This function handles both the 'continue' and the 'break' builtins that are used for loop
/// control.
fn builtin_break_continue(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let is_break = argv[0] == L!("break");
    let argc = argv.len();

    if argc != 1 {
        let error_message = wgettext_fmt!(BUILTIN_ERR_UNKNOWN, argv[0], argv[1]);
        builtin_print_help_error(parser, streams, &argv[0], &error_message);
        return STATUS_INVALID_ARGS;
    }

    // Paranoia: ensure we have a real loop.
    // This is checked in the AST but we may be invoked dynamically, e.g. just via "eval break".
    let mut has_loop = false;
    for b in parser.blocks() {
        if [BlockType::while_block, BlockType::for_block].contains(&b.typ()) {
            has_loop = true;
            break;
        }
        if b.is_function_call() {
            break;
        }
    }
    if !has_loop {
        let error_message = wgettext_fmt!("%ls: Not inside of loop\n", argv[0]);
        builtin_print_help_error(parser, streams, &argv[0], &error_message);
        return STATUS_CMD_ERROR;
    }

    // Mark the status in the libdata.
    parser.libdata_mut().pods.loop_status = if is_break {
        LoopStatus::breaks
    } else {
        LoopStatus::continues
    };
    STATUS_CMD_OK
}

/// Implementation of the builtin breakpoint command, used to launch the interactive debugger.
fn builtin_breakpoint(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    let cmd = &argv[0];
    if argv.len() != 1 {
        streams.err.append(&wgettext_fmt!(
            BUILTIN_ERR_ARG_COUNT1,
            cmd,
            0,
            argv.len() - 1
        ));
        return STATUS_INVALID_ARGS;
    }

    // If we're not interactive then we can't enter the debugger. So treat this command as a no-op.
    if !parser.is_interactive() {
        return STATUS_CMD_ERROR;
    }

    // Ensure we don't allow creating a breakpoint at an interactive prompt. There may be a simpler
    // or clearer way to do this but this works.
    let block1 = parser.block_at_index(1);
    if block1.map_or(true, |b| b.typ() == BlockType::breakpoint) {
        streams.err.append(&wgettext_fmt!(
            "%ls: Command not valid at and interactive prompt\n",
            cmd,
        ));
        return STATUS_ILLEGAL_CMD;
    }
    drop(block1);

    let bpb = parser.push_block(Block::breakpoint_block());
    let mut empty_io_chain = IoChain::new();
    let io_chain = if streams.io_chain.is_null() {
        &mut empty_io_chain
    } else {
        unsafe { &mut *streams.io_chain }
    };
    ffi::reader_read_ffi(
        &mut parser.pin() as *mut _ as *mut autocxx::c_void,
        autocxx::c_int(STDIN_FILENO),
        &io_chain as *const _ as *const autocxx::c_void,
    );
    parser.pop_block(bpb);
    Some(parser.get_last_status())
}

fn builtin_true(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    STATUS_CMD_OK
}

fn builtin_false(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    STATUS_CMD_OK
}

fn builtin_gettext(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [WString],
) -> Option<c_int> {
    for arg in argv {
        // todo! call gettext
        streams.out.append(arg);
    }
    STATUS_CMD_OK
}
