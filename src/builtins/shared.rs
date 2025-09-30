use super::prelude::*;
use crate::builtins::*;
use crate::common::{escape, get_by_sorted_name, str2wcstring, Named};
use crate::io::OutputStream;
use crate::parse_constants::UNKNOWN_BUILTIN_ERR_MSG;
use crate::parse_util::parse_util_argument_is_help;
use crate::parser::{Block, BlockType, LoopStatus};
use crate::proc::{no_exec, ProcStatus};
use crate::reader::reader_read;
use crate::wchar::L;
use errno::errno;
use libc::STDIN_FILENO;

use std::fs::File;
use std::io::{BufRead, BufReader, Read};
use std::os::fd::FromRawFd;

pub type BuiltinCmd = fn(&Parser, &mut IoStreams, &mut [&wstr]) -> BuiltinResult;

/// The default prompt for the read command.
pub const DEFAULT_READ_PROMPT: &wstr =
    L!("set_color green; echo -n read; set_color normal; echo -n \"> \"");

localizable_consts!(
    /// Error message on missing argument.
    pub BUILTIN_ERR_MISSING
    "%ls: %ls: option requires an argument\n"

    /// Error message on unexpected argument.
    pub BUILTIN_ERR_UNEXP_ARG
    "%ls: %ls: option does not take an argument\n"

    /// Error message on missing man page.
    pub BUILTIN_ERR_MISSING_HELP
    "fish: %ls: missing man page\nDocumentation may not be installed.\n`help %ls` will show an online version\n"

    /// Error message on multiple scope levels for variables.
    pub BUILTIN_ERR_GLOCAL
    "%ls: scope can be only one of: universal function global local\n"

    /// Error message for specifying both export and unexport to set/read.
    pub BUILTIN_ERR_EXPUNEXP
    "%ls: cannot both export and unexport\n"

    /// Error message for specifying both path and unpath to set/read.
    pub BUILTIN_ERR_PATHUNPATH
    "%ls: cannot both path and unpath\n"

    /// Error message for unknown switch.
    pub BUILTIN_ERR_UNKNOWN
    "%ls: %ls: unknown option\n"

    /// Error message for invalid bind mode name.
    pub BUILTIN_ERR_BIND_MODE
    "%ls: %ls: invalid mode name. See `help identifiers`\n"

    /// Error message when too many arguments are supplied to a builtin.
    pub BUILTIN_ERR_TOO_MANY_ARGUMENTS
    "%ls: too many arguments\n"

    /// Error message when integer expected
    pub BUILTIN_ERR_NOT_NUMBER
    "%ls: %ls: invalid integer\n"

    /// Command that requires a subcommand was invoked without a recognized subcommand.
    pub BUILTIN_ERR_MISSING_SUBCMD
    "%ls: missing subcommand\n"

    pub BUILTIN_ERR_INVALID_SUBCMD
    "%ls: %ls: invalid subcommand\n"

    /// Error messages for unexpected args.
    pub BUILTIN_ERR_ARG_COUNT0
    "%ls: missing argument\n"

    pub BUILTIN_ERR_ARG_COUNT1
    "%ls: expected %d arguments; got %d\n"

    pub BUILTIN_ERR_ARG_COUNT2
    "%ls: %ls: expected %d arguments; got %d\n"

    pub BUILTIN_ERR_MIN_ARG_COUNT1
    "%ls: expected >= %d arguments; got %d\n"

    pub BUILTIN_ERR_MAX_ARG_COUNT1
    "%ls: expected <= %d arguments; got %d\n"

    /// Error message for invalid variable name.
    pub BUILTIN_ERR_VARNAME
    "%ls: %ls: invalid variable name. See `help identifiers`\n"

    /// Error message on invalid combination of options.
    pub BUILTIN_ERR_COMBO
    "%ls: invalid option combination\n"

    pub BUILTIN_ERR_COMBO2
    "%ls: invalid option combination, %ls\n"

    pub BUILTIN_ERR_COMBO2_EXCLUSIVE
    "%ls: %ls %ls: options cannot be used together\n"

    /// The send stuff to foreground message.
    pub FG_MSG
    "Send job %d (%ls) to foreground\n"
);

// Return values (`$status` values for fish scripts) for various situations.

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Success {
    pub preserve_failure_exit_status: bool,
}

pub const SUCCESS: Success = Success {
    preserve_failure_exit_status: false,
};

// To-do: this should be a nonzero type.
pub type ErrorCode = c_int;

pub type BuiltinResult = Result<Success, ErrorCode>;

pub trait BuiltinResultExt {
    fn from_dynamic(code: c_int) -> Self;
    fn builtin_status_code(&self) -> c_int;
}

impl BuiltinResultExt for BuiltinResult {
    fn from_dynamic(code: c_int) -> Self {
        if code == 0 {
            Ok(SUCCESS)
        } else {
            Err(code)
        }
    }
    fn builtin_status_code(&self) -> c_int {
        match self {
            Ok(success) => {
                assert!(!success.preserve_failure_exit_status);
                0
            }
            Err(err) => *err,
        }
    }
}

/// The status code used for normal exit in a command.
pub const STATUS_CMD_OK: c_int = 0;

/// The status code used for failure exit in a command (but not if the args were invalid).
pub const STATUS_CMD_ERROR: c_int = 1;
/// The status code used for invalid arguments given to a command. This is distinct from valid
/// arguments that might result in a command failure. An invalid args condition is something
/// like an unrecognized flag, missing or too many arguments, an invalid integer, etc.
pub const STATUS_INVALID_ARGS: c_int = 2;

/// The status code used when a command was not found.
pub const STATUS_CMD_UNKNOWN: c_int = 127;

/// The status code used when an external command can not be run.
pub const STATUS_NOT_EXECUTABLE: c_int = 126;

/// The status code used when a wildcard had no matches.
pub const STATUS_UNMATCHED_WILDCARD: c_int = 124;
/// The status code used when illegal command name is encountered.
pub const STATUS_ILLEGAL_CMD: c_int = 123;
/// The status code used when `read` is asked to consume too much data.
pub const STATUS_READ_TOO_MUCH: c_int = 122;
/// The status code when an expansion fails, for example, "$foo["
pub const STATUS_EXPAND_ERROR: c_int = 121;

pub const STATUS_NO_VARIABLES_GIVEN: c_int = 255;

/// Data structure to describe a builtin.
struct BuiltinData {
    // Name of the builtin.
    name: &'static wstr,
    // Function pointer to the builtin implementation.
    func: BuiltinCmd,
}

// Data about all the builtin commands in fish.
// Functions that are bound to builtin_generic are handled directly by the parser.
// NOTE: These must be kept in sorted order!
const BUILTIN_DATAS: &[BuiltinData] = &[
    BuiltinData {
        name: L!("!"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("."),
        func: source::source,
    },
    BuiltinData {
        name: L!(":"),
        func: builtin_true,
    },
    BuiltinData {
        name: L!("["), // ]
        func: test::test,
    },
    BuiltinData {
        name: L!("_"),
        func: builtin_gettext,
    },
    BuiltinData {
        name: L!("abbr"),
        func: abbr::abbr,
    },
    BuiltinData {
        name: L!("and"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("argparse"),
        func: argparse::argparse,
    },
    BuiltinData {
        name: L!("begin"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("bg"),
        func: bg::bg,
    },
    BuiltinData {
        name: L!("bind"),
        func: bind::bind,
    },
    BuiltinData {
        name: L!("block"),
        func: block::block,
    },
    BuiltinData {
        name: L!("break"),
        func: builtin_break_continue,
    },
    BuiltinData {
        name: L!("breakpoint"),
        func: builtin_breakpoint,
    },
    BuiltinData {
        name: L!("builtin"),
        func: builtin::builtin,
    },
    BuiltinData {
        name: L!("case"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("cd"),
        func: cd::cd,
    },
    BuiltinData {
        name: L!("command"),
        func: command::command,
    },
    BuiltinData {
        name: L!("commandline"),
        func: commandline::commandline,
    },
    BuiltinData {
        name: L!("complete"),
        func: complete::complete,
    },
    BuiltinData {
        name: L!("contains"),
        func: contains::contains,
    },
    BuiltinData {
        name: L!("continue"),
        func: builtin_break_continue,
    },
    BuiltinData {
        name: L!("count"),
        func: count::count,
    },
    BuiltinData {
        name: L!("disown"),
        func: disown::disown,
    },
    BuiltinData {
        name: L!("echo"),
        func: echo::echo,
    },
    BuiltinData {
        name: L!("else"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("emit"),
        func: emit::emit,
    },
    BuiltinData {
        name: L!("end"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("eval"),
        func: eval::eval,
    },
    BuiltinData {
        name: L!("exec"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("exit"),
        func: exit::exit,
    },
    BuiltinData {
        name: L!("false"),
        func: builtin_false,
    },
    BuiltinData {
        name: L!("fg"),
        func: fg::fg,
    },
    BuiltinData {
        name: L!("fish_indent"),
        func: fish_indent::fish_indent,
    },
    BuiltinData {
        name: L!("fish_key_reader"),
        func: fish_key_reader::fish_key_reader,
    },
    BuiltinData {
        name: L!("for"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("function"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("functions"),
        func: functions::functions,
    },
    BuiltinData {
        name: L!("history"),
        func: history::history,
    },
    BuiltinData {
        name: L!("if"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("jobs"),
        func: jobs::jobs,
    },
    BuiltinData {
        name: L!("math"),
        func: math::math,
    },
    BuiltinData {
        name: L!("not"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("or"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("path"),
        func: path::path,
    },
    BuiltinData {
        name: L!("printf"),
        func: printf::printf,
    },
    BuiltinData {
        name: L!("pwd"),
        func: pwd::pwd,
    },
    BuiltinData {
        name: L!("random"),
        func: random::random,
    },
    BuiltinData {
        name: L!("read"),
        func: read::read,
    },
    BuiltinData {
        name: L!("realpath"),
        func: realpath::realpath,
    },
    BuiltinData {
        name: L!("return"),
        func: r#return::r#return,
    },
    BuiltinData {
        name: L!("set"),
        func: set::set,
    },
    BuiltinData {
        name: L!("set_color"),
        func: set_color::set_color,
    },
    BuiltinData {
        name: L!("source"),
        func: source::source,
    },
    BuiltinData {
        name: L!("status"),
        func: status::status,
    },
    BuiltinData {
        name: L!("string"),
        func: string::string,
    },
    BuiltinData {
        name: L!("switch"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("test"),
        func: test::test,
    },
    BuiltinData {
        name: L!("time"),
        func: builtin_generic,
    },
    BuiltinData {
        name: L!("true"),
        func: builtin_true,
    },
    BuiltinData {
        name: L!("type"),
        func: r#type::r#type,
    },
    BuiltinData {
        name: L!("ulimit"),
        func: ulimit::ulimit,
    },
    BuiltinData {
        name: L!("wait"),
        func: wait::wait,
    },
    BuiltinData {
        name: L!("while"),
        func: builtin_generic,
    },
];
assert_sorted_by_name!(BUILTIN_DATAS);

impl Named for BuiltinData {
    fn name(&self) -> &'static wstr {
        self.name
    }
}

fn builtin_lookup(name: &wstr) -> Option<&'static BuiltinData> {
    get_by_sorted_name(name, BUILTIN_DATAS)
}

/// Is there a builtin command with the given name?
pub fn builtin_exists(name: &wstr) -> bool {
    builtin_lookup(name).is_some()
}

/// Is the command a keyword we need to special-case the handling of `-h` and `--help`.
fn cmd_needs_help(cmd: &wstr) -> bool {
    [
        L!("for"),
        L!("while"),
        L!("function"),
        L!("if"),
        L!("end"),
        L!("switch"),
        L!("case"),
    ]
    .contains(&cmd)
}

/// Execute a builtin command
pub fn builtin_run(parser: &Parser, argv: &mut [&wstr], streams: &mut IoStreams) -> ProcStatus {
    if argv.is_empty() {
        return ProcStatus::from_exit_code(STATUS_INVALID_ARGS);
    }

    // We can be handed a keyword by the parser as if it was a command. This happens when the user
    // follows the keyword by `-h` or `--help`. Since it isn't really a builtin command we need to
    // handle displaying help for it here.
    if argv.len() == 2 && parse_util_argument_is_help(argv[1]) && cmd_needs_help(argv[0]) {
        builtin_print_help(parser, streams, argv[0]);
        return ProcStatus::from_exit_code(STATUS_CMD_OK);
    }

    let Some(builtin) = builtin_lookup(argv[0]) else {
        FLOGF!(error, "%s", wgettext_fmt!(UNKNOWN_BUILTIN_ERR_MSG, argv[0]));
        return ProcStatus::from_exit_code(STATUS_CMD_ERROR);
    };

    let builtin_ret = (builtin.func)(parser, streams, argv);

    // Flush our out and error streams, and check for their errors.
    let out_ret = streams.out.flush_and_check_error();
    let err_ret = streams.err.flush_and_check_error();

    // Resolve our status code.
    // If the builtin itself produced an error, use that error.
    // Otherwise use any errors from writing to out and writing to err, in that order.
    let mut code = match builtin_ret {
        Ok(success) => {
            if success.preserve_failure_exit_status {
                return ProcStatus::empty();
            } else {
                0
            }
        }
        Err(err) => err,
    };

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

    if code < 0 {
        // If the code is below 0, constructing a proc_status_t
        // would assert() out, which is a terrible failure mode
        // So instead, what we do is we get a positive code,
        // and we avoid 0.
        FLOGF!(
            warning,
            "builtin %ls returned invalid exit code %d",
            argv[0],
            code
        );
        code = ((256 + code) % 256).abs();
        if code == 0 {
            code = 255;
        }
    }

    ProcStatus::from_exit_code(code)
}

/// Returns a list of all builtin names.
pub fn builtin_get_names() -> impl Iterator<Item = &'static wstr> {
    BUILTIN_DATAS.iter().map(|builtin| builtin.name)
}

/// Return a one-line description of the specified builtin.
pub fn builtin_get_desc(name: &wstr) -> Option<&'static wstr> {
    let desc = match name {
        _ if name == "!" => wgettext!("Negate exit status of job"),
        _ if name == "." => wgettext!("Evaluate contents of file"),
        _ if name == ":" => wgettext!("Return a successful result"),
        _ if name == "[" => wgettext!("Test a condition"), // ]
        _ if name == "_" => wgettext!("Translate a string"),
        _ if name == "abbr" => wgettext!("Manage abbreviations"),
        _ if name == "and" => wgettext!("Run command if last command succeeded"),
        _ if name == "argparse" => wgettext!("Parse options in fish script"),
        _ if name == "begin" => wgettext!("Create a block of code"),
        _ if name == "bg" => wgettext!("Send job to background"),
        _ if name == "bind" => wgettext!("Handle fish key bindings"),
        _ if name == "block" => wgettext!("Temporarily block delivery of events"),
        _ if name == "break" => wgettext!("Stop the innermost loop"),
        _ if name == "breakpoint" => wgettext!("Halt execution and start debug prompt"),
        _ if name == "builtin" => wgettext!("Run a builtin specifically"),
        _ if name == "case" => wgettext!("Block of code to run conditionally"),
        _ if name == "cd" => wgettext!("Change working directory"),
        _ if name == "command" => wgettext!("Run a command specifically"),
        _ if name == "commandline" => wgettext!("Set or get the commandline"),
        _ if name == "complete" => wgettext!("Edit command specific completions"),
        _ if name == "contains" => wgettext!("Search for a specified string in a list"),
        _ if name == "continue" => wgettext!("Skip over remaining innermost loop"),
        _ if name == "count" => wgettext!("Count the number of arguments"),
        _ if name == "disown" => wgettext!("Remove job from job list"),
        _ if name == "echo" => wgettext!("Print arguments"),
        _ if name == "else" => wgettext!("Evaluate block if condition is false"),
        _ if name == "emit" => wgettext!("Emit an event"),
        _ if name == "end" => wgettext!("End a block of commands"),
        _ if name == "eval" => wgettext!("Evaluate a string as a statement"),
        _ if name == "exec" => wgettext!("Run command in current process"),
        _ if name == "exit" => wgettext!("Exit the shell"),
        _ if name == "false" => wgettext!("Return an unsuccessful result"),
        _ if name == "fg" => wgettext!("Send job to foreground"),
        _ if name == "fish_key_reader" => wgettext!("explore what characters keyboard keys send"),
        _ if name == "for" => wgettext!("Perform a set of commands multiple times"),
        _ if name == "function" => wgettext!("Define a new function"),
        _ if name == "functions" => wgettext!("List or remove functions"),
        _ if name == "history" => wgettext!("History of commands executed by user"),
        _ if name == "if" => wgettext!("Evaluate block if condition is true"),
        _ if name == "jobs" => wgettext!("Print currently running jobs"),
        _ if name == "math" => wgettext!("Evaluate math expressions"),
        _ if name == "not" => wgettext!("Negate exit status of job"),
        _ if name == "or" => wgettext!("Execute command if previous command failed"),
        _ if name == "path" => wgettext!("Handle paths"),
        _ if name == "printf" => wgettext!("Prints formatted text"),
        _ if name == "pwd" => wgettext!("Print the working directory"),
        _ if name == "random" => wgettext!("Generate random number"),
        _ if name == "read" => wgettext!("Read a line of input into variables"),
        _ if name == "realpath" => wgettext!("Show absolute path sans symlinks"),
        _ if name == "return" => wgettext!("Stop the currently evaluated function"),
        _ if name == "set" => wgettext!("Handle environment variables"),
        _ if name == "set_color" => wgettext!("Set the terminal color"),
        _ if name == "source" => wgettext!("Evaluate contents of file"),
        _ if name == "status" => wgettext!("Return status information about fish"),
        _ if name == "string" => wgettext!("Manipulate strings"),
        _ if name == "switch" => wgettext!("Conditionally run blocks of code"),
        _ if name == "test" => wgettext!("Test a condition"),
        _ if name == "time" => wgettext!("Measure how long a command or block takes"),
        _ if name == "true" => wgettext!("Return a successful result"),
        _ if name == "type" => wgettext!("Check if a thing is a thing"),
        _ if name == "ulimit" => wgettext!("Get/set resource usage limits"),
        _ if name == "wait" => wgettext!("Await background process completion"),
        _ if name == "while" => wgettext!("Perform a command multiple times"),
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
pub fn builtin_print_help(parser: &Parser, streams: &mut IoStreams, cmd: &wstr) {
    // This won't ever work if no_exec is set.
    if no_exec() {
        return;
    }
    let name_esc = escape(cmd);
    let cmd = sprintf!("__fish_print_help %ls ", &name_esc);
    let res = parser.eval(&cmd, streams.io_chain);
    if res.status.normal_exited() && res.status.exit_code() == 2 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_MISSING_HELP, name_esc, name_esc));
    }
}

/// Perform error reporting for encounter with unknown option.
pub fn builtin_unknown_option(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    opt: &wstr,
    print_hints: bool, /*=true*/
) {
    streams
        .err
        .append(wgettext_fmt!(BUILTIN_ERR_UNKNOWN, cmd, opt));
    if print_hints {
        builtin_print_error_trailer(parser, streams.err, cmd);
    }
}

/// Perform error reporting for encounter with missing argument.
pub fn builtin_missing_argument(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    mut opt: &wstr,
    print_hints: bool, /*=true*/
) {
    if opt.char_at(0) == '-' && opt.char_at(1) != '-' {
        // if c in -qc '-qc' is missing the argument, now opt is just 'c'
        opt = &opt[opt.len() - 1..];
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_MISSING,
            cmd,
            L!("-").to_owned() + opt
        ));
    } else {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_MISSING, cmd, opt));
    }
    if print_hints {
        builtin_print_error_trailer(parser, streams.err, cmd);
    }
}

/// Perform error reporting for encounter with an extra argument.
pub fn builtin_unexpected_argument(
    parser: &Parser,
    streams: &mut IoStreams,
    cmd: &wstr,
    opt: &wstr,
    print_hints: bool, /*=true*/
) {
    streams
        .err
        .append(wgettext_fmt!(BUILTIN_ERR_UNEXP_ARG, cmd, opt));
    if print_hints {
        builtin_print_error_trailer(parser, streams.err, cmd);
    }
}

/// Print the backtrace and call for help that we use at the end of error messages.
pub fn builtin_print_error_trailer(parser: &Parser, b: &mut OutputStream, cmd: &wstr) {
    b.push('\n');
    let stacktrace = parser.current_line();
    // Don't print two empty lines if we don't have a stacktrace.
    if !stacktrace.is_empty() {
        b.append(stacktrace);
        b.push('\n');
    }
    b.append(wgettext_fmt!(
        "(Type 'help %ls' for related documentation)\n",
        cmd
    ));
}

/// This function works like perror, but it prints its result into the streams.err string instead
/// to stderr. Used by the builtin commands.
pub fn builtin_wperror(program_name: &wstr, streams: &mut IoStreams) {
    let err = errno();
    streams.err.append(program_name);
    streams.err.append(L!(": "));
    if err.0 != 0 {
        let werr = WString::from_str(&err.to_string());
        streams.err.append(werr);
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
        parser: &Parser,
        streams: &mut IoStreams,
    ) -> Result<Self, ErrorCode> {
        let cmd = args[0];
        let print_hints = true;

        const shortopts: &wstr = L!("+h");
        const longopts: &[WOption] = &[wopt(L!("help"), ArgType::NoArgument, 'h')];

        let mut print_help = false;
        let mut w = WGetopter::new(shortopts, longopts, args);
        while let Some(c) = w.next_opt() {
            match c {
                'h' => {
                    print_help = true;
                }
                ':' => {
                    builtin_missing_argument(
                        parser,
                        streams,
                        cmd,
                        args[w.wopt_index - 1],
                        print_hints,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                ';' => {
                    builtin_unexpected_argument(
                        parser,
                        streams,
                        cmd,
                        args[w.wopt_index - 1],
                        print_hints,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                '?' => {
                    builtin_unknown_option(
                        parser,
                        streams,
                        cmd,
                        args[w.wopt_index - 1],
                        print_hints,
                    );
                    return Err(STATUS_INVALID_ARGS);
                }
                _ => {
                    panic!("unexpected retval from WGetopter");
                }
            }
        }

        Ok(HelpOnlyCmdOpts {
            print_help,
            optind: w.wopt_index,
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
        streams: &mut IoStreams,
        chunk_size: usize,
    ) -> Self {
        let reader = streams.stdin_is_directly_redirected.then(|| {
            let stdin_fd = streams.stdin_fd;
            assert!(stdin_fd >= 0, "should have a valid fd");
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
        let (end, want_newline) = match (&self.split_behavior, self.buffer.last()) {
            // remove the newline — consumers do not expect it
            (Newline, Some(b'\n')) => (num_bytes - 1, true),
            // we are missing a trailing newline!
            (Newline, _) => (num_bytes, false),
            // consumers do not expect to deal with the null
            // "want_newline" is not currently relevant for Null
            (Null, Some(b'\0')) => (num_bytes - 1, false),
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

/// A generic builtin that only supports showing a help message. This is only a placeholder that
/// prints the help message. Useful for commands that live in the parser.
fn builtin_generic(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let argc = argv.len();
    let opts = HelpOnlyCmdOpts::parse(argv, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, argv[0]);
        return Ok(SUCCESS);
    }

    // Hackish - if we have no arguments other than the command, we are a "naked invocation" and we
    // just print help.
    if argc == 1 || argv[0] == "time" {
        builtin_print_help(parser, streams, argv[0]);
        return Err(STATUS_INVALID_ARGS);
    }

    Err(STATUS_CMD_ERROR)
}

/// This function handles both the 'continue' and the 'break' builtins that are used for loop
/// control.
fn builtin_break_continue(
    parser: &Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> BuiltinResult {
    let is_break = argv[0] == "break";
    let argc = argv.len();

    let opts = HelpOnlyCmdOpts::parse(argv, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, argv[0]);
        return Ok(SUCCESS);
    }

    if argc != 1 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_UNKNOWN, argv[0], argv[1]));
        return Err(STATUS_INVALID_ARGS);
    }

    // Paranoia: ensure we have a real loop.
    // This is checked in the AST but we may be invoked dynamically, e.g. just via "eval break".
    let mut has_loop = false;
    for b in parser.blocks_iter_rev() {
        if [BlockType::while_block, BlockType::for_block].contains(&b.typ()) {
            has_loop = true;
            break;
        }
        if b.is_function_call() {
            break;
        }
    }
    if !has_loop {
        streams
            .err
            .append(wgettext_fmt!("%ls: Not inside of loop\n", argv[0]));
        return Err(STATUS_CMD_ERROR);
    }

    // Mark the status in the libdata.
    parser.libdata_mut().loop_status = if is_break {
        LoopStatus::breaks
    } else {
        LoopStatus::continues
    };
    Ok(SUCCESS)
}

/// Implementation of the builtin breakpoint command, used to launch the interactive debugger.
fn builtin_breakpoint(
    parser: &Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> BuiltinResult {
    let cmd = argv[0];
    if argv.len() != 1 {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_ARG_COUNT1,
            cmd,
            0,
            argv.len() - 1
        ));
        return Err(STATUS_INVALID_ARGS);
    }

    // If we're not interactive then we can't enter the debugger. So treat this command as a no-op.
    if !parser.is_interactive() {
        return Err(STATUS_CMD_ERROR);
    }

    // Ensure we don't allow creating a breakpoint at an interactive prompt. There may be a simpler
    // or clearer way to do this but this works.
    {
        if parser
            .block_at_index(1)
            .map_or(true, |b| b.typ() == BlockType::breakpoint)
        {
            streams.err.append(wgettext_fmt!(
                "%ls: Command not valid at an interactive prompt\n",
                cmd,
            ));
            return Err(STATUS_ILLEGAL_CMD);
        }
    }

    let bpb = parser.push_block(Block::breakpoint_block());
    let io_chain = &streams.io_chain;
    reader_read(parser, STDIN_FILENO, io_chain)?;
    parser.pop_block(bpb);
    BuiltinResult::from_dynamic(parser.get_last_status())
}

fn builtin_true(_parser: &Parser, _streams: &mut IoStreams, _argv: &mut [&wstr]) -> BuiltinResult {
    Ok(SUCCESS)
}

fn builtin_false(_parser: &Parser, _streams: &mut IoStreams, _argv: &mut [&wstr]) -> BuiltinResult {
    Err(STATUS_CMD_ERROR)
}

/// Used for the fish `_` builtin for requesting translations.
/// For scripts in `share/`, the corresponding strings are extracted from the scripts using
/// `build_tools/fish_xgettext.fish`.
/// Strings not present in our repo would require a custom MO file for translation to be possible.
fn builtin_gettext(_parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    for arg in &argv[1..] {
        streams.out.append(
            crate::wutil::LocalizableString::from_external_source((*arg).to_owned()).localize(),
        );
    }
    Ok(SUCCESS)
}
