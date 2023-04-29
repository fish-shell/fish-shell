use crate::builtins::{printf, wait};
use crate::common::escape;
use crate::ffi::{self, wcstring_list_ffi_t, Repin};
use crate::io::IoStreams;
use crate::parser::{Block, LoopStatus, Parser};
use crate::proc::{no_exec, ProcStatus};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ffi::{c_str, empty_wstring, WCharFromFFI};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::wgettext_fmt;
use libc::c_int;
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
    // Description of what the builtin does.
    desc: &'static wstr,
}

impl BuiltinData {
    fn new(name: &'static wstr, func: BuiltinImplementation, desc: &'static wstr) -> Self {
        Self { name, func, desc }
    }
}

enum BuiltinImplementation {
    Rust(fn(parser: &mut Parser, streams: &mut IoStreams<'_>, argv: &mut [&wstr]) -> Option<c_int>),
    Cpp(fn(parser: Pin<&mut Parser>, streams: &mut IoStreams<'_>, argv: *mut *const char)),
}

// Data about all the builtin commands in fish.
// Functions that are bound to builtin_generic are handled directly by the parser.
// NOTE: These must be kept in sorted order!
#[rustfmt::skip]
const BUILTIN_DATAS: &[BuiltinData] = &[
    BuiltinData::new("."L, BuiltinImplementation::Rust(builtin_source), "Evaluate contents of file"L),
    BuiltinData::new(":"L, BuiltinImplementation::Rust(builtin_true), "Return a successful result"L),
    BuiltinData::new("["L, BuiltinImplementation::Cpp(ffi::builtin_test), "Test a condition"L), // ]
    BuiltinData::new("_"L, BuiltinImplementation::Rust(builtin_gettext), "Translate a string"L),
    BuiltinData::new("abbr"L, BuiltinImplementation::Cpp(crate::builtins::abbr::abbr), "Manage abbreviations"L),
    BuiltinData::new("and"L, BuiltinImplementation::Rust(builtin_generic), "Run command if last command succeeded"L),
    BuiltinData::new("argparse"L, BuiltinImplementation::Cpp(ffi::builtin_argparse), "Parse options in fish script"L),
    BuiltinData::new("begin"L, BuiltinImplementation::Rust(builtin_generic), "Create a block of code"L),
    BuiltinData::new("bg"L, BuiltinImplementation::Cpp(crate::builtins::bg::bg), "Send job to background"L),
    BuiltinData::new("bind"L, BuiltinImplementation::Cpp(ffi::builtin_bind), "Handle fish key bindings"L),
    BuiltinData::new("block"L, BuiltinImplementation::Cpp(crate::builtins::block::block), "Temporarily block delivery of events"L),
    BuiltinData::new("break"L, BuiltinImplementation::Rust(builtin_break_continue), "Stop the innermost loop"L),
    BuiltinData::new("breakpoint"L, BuiltinImplementation::Rust(builtin_breakpoint), "Halt execution and start debug prompt"L),
    BuiltinData::new("builtin"L, BuiltinImplementation::Cpp(crate::builtins::builtin::builtin), "Run a builtin specifically"L),
    BuiltinData::new("case"L, BuiltinImplementation::Rust(builtin_generic), "Block of code to run conditionally"L),
    BuiltinData::new("cd"L, BuiltinImplementation::Cpp(ffi::builtin_cd), "Change working directory"L),
    BuiltinData::new("command"L, BuiltinImplementation::Cpp(crate::builtins::command::command), "Run a command specifically"L),
    BuiltinData::new("commandline"L, BuiltinImplementation::Cpp(ffi::builtin_commandline), "Set or get the commandline"L),
    BuiltinData::new("complete"L, BuiltinImplementation::Cpp(ffi::builtin_complete), "Edit command specific completions"L),
    BuiltinData::new("contains"L, BuiltinImplementation::Cpp(crate::builtins::contains::contains), "Search for a specified string in a list"L),
    BuiltinData::new("continue"L, BuiltinImplementation::Rust(builtin_break_continue), "Skip over remaining innermost loop"L),
    BuiltinData::new("count"L, BuiltinImplementation::Rust(builtin_count), "Count the number of arguments"L),
    BuiltinData::new("disown"L, BuiltinImplementation::Cpp(ffi::builtin_disown), "Remove job from job list"L),
    BuiltinData::new("echo"L, BuiltinImplementation::Cpp(crate::builtins::echo::echo), "Print arguments"L),
    BuiltinData::new("else"L, BuiltinImplementation::Rust(builtin_generic), "Evaluate block if condition is false"L),
    BuiltinData::new("emit"L, BuiltinImplementation::Cpp(crate::builtins::emit::emit), "Emit an event"L),
    BuiltinData::new("end"L, BuiltinImplementation::Rust(builtin_generic), "End a block of commands"L),
    BuiltinData::new("eval"L, BuiltinImplementation::Cpp(ffi::builtin_eval), "Evaluate a string as a statement"L),
    BuiltinData::new("exec"L, BuiltinImplementation::Rust(builtin_generic), "Run command in current process"L),
    BuiltinData::new("exit"L, BuiltinImplementation::Cpp(crate::builtins::exit::exit), "Exit the shell"L),
    BuiltinData::new("false"L, BuiltinImplementation::Rust(builtin_false), "Return an unsuccessful result"L),
    BuiltinData::new("fg"L, BuiltinImplementation::Cpp(ffi::builtin_fg), "Send job to foreground"L),
    BuiltinData::new("for"L, BuiltinImplementation::Rust(builtin_generic), "Perform a set of commands multiple times"L),
    BuiltinData::new("function"L, BuiltinImplementation::Rust(builtin_generic), "Define a new function"L),
    BuiltinData::new("functions"L, BuiltinImplementation::Cpp(ffi::builtin_functions), "List or remove functions"L),
    BuiltinData::new("history"L, BuiltinImplementation::Cpp(ffi::builtin_history), "History of commands executed by user"L),
    BuiltinData::new("if"L, BuiltinImplementation::Rust(builtin_generic), "Evaluate block if condition is true"L),
    BuiltinData::new("jobs"L, BuiltinImplementation::Cpp(ffi::builtin_jobs), "Print currently running jobs"L),
    BuiltinData::new("math"L, BuiltinImplementation::Cpp(crate::builtins::math::math), "Evaluate math expressions"L),
    BuiltinData::new("not"L, BuiltinImplementation::Rust(builtin_generic), "Negate exit status of job"L),
    BuiltinData::new("or"L, BuiltinImplementation::Rust(builtin_generic), "Execute command if previous command failed"L),
    BuiltinData::new("path"L, BuiltinImplementation::Cpp(ffi::builtin_path), "Handle paths"L),
    BuiltinData::new("printf"L, BuiltinImplementation::Cpp(crate::builtins::printf::printf), "Prints formatted text"L),
    BuiltinData::new("pwd"L, BuiltinImplementation::Cpp(crate::builtins::pwd::pwd), "Print the working directory"L),
    BuiltinData::new("random"L, BuiltinImplementation::Cpp(crate::builtins::random::random), "Generate random number"L),
    BuiltinData::new("read"L, BuiltinImplementation::Cpp(ffi::builtin_read), "Read a line of input into variables"L),
    BuiltinData::new("realpath"L, BuiltinImplementation::Cpp(crate::builtins::realpath::realpath), "Show absolute path sans symlinks"L),
    BuiltinData::new("return"L, BuiltinImplementation::Cpp(crate::builtins::r#return::r#return), "Stop the currently evaluated function"L),
    BuiltinData::new("set"L, BuiltinImplementation::Cpp(ffi::builtin_set), "Handle environment variables"L),
    BuiltinData::new("set_color"L, BuiltinImplementation::Cpp(ffi::builtin_set_color), "Set the terminal color"L),
    BuiltinData::new("source"L, BuiltinImplementation::Cpp(ffi::builtin_source), "Evaluate contents of file"L),
    BuiltinData::new("status"L, BuiltinImplementation::Cpp(ffi::builtin_status), "Return status information about fish"L),
    BuiltinData::new("string"L, BuiltinImplementation::Cpp(ffi::builtin_string), "Manipulate strings"L),
    BuiltinData::new("switch"L, BuiltinImplementation::Rust(builtin_generic), "Conditionally run blocks of code"L),
    BuiltinData::new("test"L, BuiltinImplementation::Cpp(ffi::builtin_test), "Test a condition"L),
    BuiltinData::new("time"L, BuiltinImplementation::Rust(builtin_generic), "Measure how long a command or block takes"L),
    BuiltinData::new("true"L, BuiltinImplementation::Cpp(ffi::builtin_true), "Return a successful result"L),
    BuiltinData::new("type"L, BuiltinImplementation::Cpp(crate::builtins::r#type::r#type), "Check if a thing is a thing"L),
    BuiltinData::new("ulimit"L, BuiltinImplementation::Cpp(ffi::builtin_ulimit), "Get/set resource usage limits"L),
    BuiltinData::new("wait"L, BuiltinImplementation::Cpp(crate::builtins::wait::wait), "Wait for background processes completed"L),
    BuiltinData::new("while"L, BuiltinImplementation::Rust(builtin_generic), "Perform a command multiple times"L),
];
assert_sorted_by_name!(BUILTIN_DATAS);

/// Look up a builtin_data_t for a specified builtin
///
/// @param  name
///    Name of the builtin
///
/// @return
///    Pointer to a builtin_data_t
///
pub fn builtin_lookup(name: &wstr) -> Option<&'static BuiltinData> {
    get_by_sorted_name(name, builtin_datas)
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

/// Execute a builtin command
pub fn builtin_run<S: AsRef<wstr>>(
    parser: &Parser,
    argv: &[S],
    streams: &IoStreams<'_>,
) -> ProcStatus {
    if argv.is_empty() {
        return ProcStatus::from_exit_code(STATUS_INVALID_ARGS.unwrap());
    }
    let cmdname = argv[0];

    // We can be handed a keyword by the parser as if it was a command. This happens when the user
    // follows the keyword by `-h` or `--help`. Since it isn't really a builtin command we need to
    // handle displaying help for it here.
    if argv.len() == 2 && parse_util_argument_is_help(argv[1]) && cmd_needs_help(cmdname) {
        builtin_print_help(parser, streams, cmdname);
        return ProcStatus::from_exit_code(STATUS_CMD_OK.unwrap());
    }

    let Some(builtin) = builtin_lookup(cmdname) else {
        FLOGF!(error, "%s", wgettext_fmt!(UNKNOWN_BUILTIN_ERR_MSG, cmdname));
        return ProcStatus::from_exit_code(STATUS_CMD_ERROR.unwrap());
    };

    let builtin_ret = match &builtin.func {
        BuiltinImplementation::Cpp(func) => {
            // Construct the permutable argv array which the builtin expects, and execute the builtin.
            let arg_arr = OwningNullTerminatedArray::new(argv)
            let ret = data.func(parser, streams, argv_arr);
            if ret.is_null() { None } else { Some(*ret) }
        }
        BuiltinImplementation::Rust(func) => (func)(parser, streams, args),
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
            cmdname,
            code
        );
    }

    ProcStatus::from_exit_code(code)
}

/// Returns a list of all builtin names.
pub fn builtin_get_names() -> impl Iterator<Item = &'static wstr> {
    BUILTIN_DATAS.map(|builtin| builtin.name)
}

/// Return a one-line description of the specified builtin.
pub fn builtin_get_desc(name: &wstr) -> Option<&'static wstr> {
    builtin_lookup(name).map(|builtin| wgettext!(builtin.desc))
}

/// Display help/usage information for the specified builtin or function from manpage
///
/// @param  name
///    builtin or function name to get up help for
///
/// Process and print help for the specified builtin or function.
pub fn builtin_print_help(parser: &mut Parser, streams: &IoStreams<'_>, cmd: &wstr) {
    builtin_print_help_error(parser, streams, cmd, L!(""))
}
pub fn builtin_print_help_error(
    parser: &mut Parser,
    streams: &IoStreams<'_>,
    cmd: &wstr,
    error_message: &wstr,
) {
    // This won't ever work if no_exec is set.
    if no_exec() {
        return;
    }
    let name_esc = escape(name);
    let cmd = sprintf!("__fish_print_help %ls ", &name_esc);
    let mut ios = IoChain::new();
    if !error_message.is_empty() {
        cmd.append(&escape(error_message));
        // If it's an error, redirect the output of __fish_print_help to stderr
        ios.push(Arc::new(IoFd::new(STDOUT_FILENO, STDERR_FILENO)));
    }
    let ret = parser.eval(cmd, ios);
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
    if opt.chars().take(2).any(|c| *c == '-') {
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
pub fn builtin_print_error_trailer(parser: &mut Parser, b: &dyn OutputStream, cmd: &wstr) {
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
        let werr = str2wcstring(err.to_string());
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

/// A generic builtin that only supports showing a help message. This is only a placeholder that
/// prints the help message. Useful for commands that live in the parser.
fn builtin_generic(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [&wstr],
) -> Option<c_int> {
    let cmd = argv[0];
    let argc = argv.len();
    let mut opts = HelpOnlyCmdOpts::new();
    let mut optind;
    let opts = match HelpOnlyCmdOpts::parse(argv, parser, streams) {
        Ok(opts) => opts,
        Err(err) => return Some(err),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // Hackish - if we have no arguments other than the command, we are a "naked invocation" and we
    // just print help.
    if argc == 1 || cmd == L!("time") {
        builtin_print_help(parser, streams, cmd);
        return STATUS_INVALID_ARGS;
    }

    STATUS_CMD_ERROR
}

// How many bytes we read() at once.
// Since this is just for counting, it can be massive.
const COUNT_CHUNK_SIZE: usize = (512 * 256);
/// Implementation of the builtin count command, used to count the number of arguments sent to it.
fn builtin_count(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [&wstr],
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
            let n = read_blocked(streams.stdin_fd, &mut buf, COUNT_CHUNK_SIZE);
            if n == 0 {
                break;
            } else if n < 0 {
                perror("read");
                return STATUS_CMD_ERROR;
            }
            buf[..n].iter().filter(|c| *c == b'\n').count();
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
    argv: &mut [&wstr],
) -> Option<c_int> {
    let is_break = argv[0] == L!("break");
    let argc = argv.len();

    if argc != 1 {
        let error_message = wgettext_fmt!(BUILTIN_ERR_UNKNOWN, argv[0], argv[1]);
        builtin_print_help_error(parser, streams, argv[0], &error_message);
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
        builtin_print_help_error(parser, streams, argv[0], &error_message);
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
    argv: &mut [&wstr],
) -> Option<c_int> {
    let cmd = argv[0];
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
        streams.err.append(
            wgettext_fmt!("%ls: Command not valid at and interactive prompt\n"),
            cmd,
        );
        return STATUS_ILLEGAL_CMD;
    }
    drop(block1);

    let bpb = parser.push_block(Block::breakpoint_block());
    reader_read(
        parser,
        STDIN_FILENO,
        &if streams.io.is_null() {
            IoChain::new()
        } else {
            unsafe { &*streams.io_chain }
        },
    );
    parser.pop_block(bpb);
    Some(parser.get_last_status())
}

fn builtin_true(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [&wstr],
) -> Option<c_int> {
    STATUS_CMD_OK
}

fn builtin_false(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [&wstr],
) -> Option<c_int> {
    STATUS_CMD_OK
}

fn builtin_gettext(
    parser: &mut Parser,
    streams: &mut IoStreams<'_>,
    argv: &mut [&wstr],
) -> Option<c_int> {
    for arg in argv {
        strams.out.append(wgettext(arg));
    }
    STATUS_CMD_OK
}
