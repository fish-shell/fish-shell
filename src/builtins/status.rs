use std::os::unix::prelude::*;

use super::prelude::*;
use crate::common::{get_executable_path, str2wcstring, PROGRAM_NAME};
use crate::future_feature_flags::{self as features, feature_test};
use crate::proc::{
    get_job_control_mode, get_login, is_interactive_session, set_job_control_mode, JobControl,
};
use crate::wutil::{waccess, wbasename, wdirname, wrealpath, Error};
use libc::F_OK;
use nix::errno::Errno;
use nix::NixPath;

macro_rules! str_enum {
    ($name:ident, $(($val:ident, $str:expr)),* $(,)?) => {
        impl $name {
            fn from_wstr(s: &str) -> Option<Self> {
                // matching on str's lets us avoid having to do binary search and friends ourselves,
                // this is ascii only anyways
                match s {
                    $($str => Some(Self::$val)),*,
                    _ => None,
                }
            }

            fn to_wstr(self) -> &'static wstr {
                // There can be multiple vals => str mappings, and that's okay
                #[allow(unreachable_patterns)]
                match self {
                    $(Self::$val => L!($str)),*,
                }
            }
        }
    }
}

use StatusCmd::*;
#[derive(Clone, Copy)]
enum StatusCmd {
    STATUS_CURRENT_CMD = 1,
    STATUS_BASENAME,
    STATUS_DIRNAME,
    STATUS_FEATURES,
    STATUS_FILENAME,
    STATUS_FISH_PATH,
    STATUS_FUNCTION,
    STATUS_IS_BLOCK,
    STATUS_IS_BREAKPOINT,
    STATUS_IS_COMMAND_SUB,
    STATUS_IS_FULL_JOB_CTRL,
    STATUS_IS_INTERACTIVE,
    STATUS_IS_INTERACTIVE_JOB_CTRL,
    STATUS_IS_LOGIN,
    STATUS_IS_NO_JOB_CTRL,
    STATUS_LINE_NUMBER,
    STATUS_SET_JOB_CONTROL,
    STATUS_STACK_TRACE,
    STATUS_TEST_FEATURE,
    STATUS_CURRENT_COMMANDLINE,
}

str_enum!(
    StatusCmd,
    (STATUS_BASENAME, "basename"),
    (STATUS_BASENAME, "current-basename"),
    (STATUS_CURRENT_CMD, "current-command"),
    (STATUS_CURRENT_COMMANDLINE, "current-commandline"),
    (STATUS_DIRNAME, "current-dirname"),
    (STATUS_FILENAME, "current-filename"),
    (STATUS_FUNCTION, "current-function"),
    (STATUS_LINE_NUMBER, "current-line-number"),
    (STATUS_DIRNAME, "dirname"),
    (STATUS_FEATURES, "features"),
    (STATUS_FILENAME, "filename"),
    (STATUS_FISH_PATH, "fish-path"),
    (STATUS_FUNCTION, "function"),
    (STATUS_IS_BLOCK, "is-block"),
    (STATUS_IS_BREAKPOINT, "is-breakpoint"),
    (STATUS_IS_COMMAND_SUB, "is-command-substitution"),
    (STATUS_IS_FULL_JOB_CTRL, "is-full-job-control"),
    (STATUS_IS_INTERACTIVE, "is-interactive"),
    (STATUS_IS_INTERACTIVE_JOB_CTRL, "is-interactive-job-control"),
    (STATUS_IS_LOGIN, "is-login"),
    (STATUS_IS_NO_JOB_CTRL, "is-no-job-control"),
    (STATUS_SET_JOB_CONTROL, "job-control"),
    (STATUS_LINE_NUMBER, "line-number"),
    (STATUS_STACK_TRACE, "print-stack-trace"),
    (STATUS_STACK_TRACE, "stack-trace"),
    (STATUS_TEST_FEATURE, "test-feature"),
);

/// Values that may be returned from the test-feature option to status.
#[repr(i32)]
enum TestFeatureRetVal {
    TEST_FEATURE_ON = 0,
    TEST_FEATURE_OFF,
    TEST_FEATURE_NOT_RECOGNIZED,
}

struct StatusCmdOpts {
    level: i32,
    new_job_control_mode: Option<JobControl>,
    status_cmd: Option<StatusCmd>,
    print_help: bool,
}

impl StatusCmdOpts {
    // Set the status command to the given command.
    // Returns true on success, false if there's already a status command, in which case an error is printed.
    fn try_set_status_cmd(&mut self, subcmd: StatusCmd, streams: &mut IoStreams) -> bool {
        match self.status_cmd.replace(subcmd) {
            Some(existing) => {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_COMBO2_EXCLUSIVE,
                    "status",
                    existing.to_wstr(),
                    subcmd.to_wstr(),
                ));
                false
            }
            None => true,
        }
    }
}

impl Default for StatusCmdOpts {
    fn default() -> Self {
        Self {
            level: 1,
            new_job_control_mode: None,
            status_cmd: None,
            print_help: false,
        }
    }
}

/// Short options for long options that have no corresponding short options.
const FISH_PATH_SHORT: char = '\x01';
const IS_FULL_JOB_CTRL_SHORT: char = '\x02';
const IS_INTERACTIVE_JOB_CTRL_SHORT: char = '\x03';
const IS_NO_JOB_CTRL_SHORT: char = '\x04';

const SHORT_OPTIONS: &wstr = L!(":L:cbilfnhj:t");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("help"), NoArgument, 'h'),
    wopt(L!("current-filename"), NoArgument, 'f'),
    wopt(L!("current-line-number"), NoArgument, 'n'),
    wopt(L!("filename"), NoArgument, 'f'),
    wopt(L!("fish-path"), NoArgument, FISH_PATH_SHORT),
    wopt(L!("is-block"), NoArgument, 'b'),
    wopt(L!("is-command-substitution"), NoArgument, 'c'),
    wopt(
        L!("is-full-job-control"),
        NoArgument,
        IS_FULL_JOB_CTRL_SHORT,
    ),
    wopt(L!("is-interactive"), NoArgument, 'i'),
    wopt(
        L!("is-interactive-job-control"),
        NoArgument,
        IS_INTERACTIVE_JOB_CTRL_SHORT,
    ),
    wopt(L!("is-login"), NoArgument, 'l'),
    wopt(L!("is-no-job-control"), NoArgument, IS_NO_JOB_CTRL_SHORT),
    wopt(L!("job-control"), RequiredArgument, 'j'),
    wopt(L!("level"), RequiredArgument, 'L'),
    wopt(L!("line"), NoArgument, 'n'),
    wopt(L!("line-number"), NoArgument, 'n'),
    wopt(L!("print-stack-trace"), NoArgument, 't'),
];

/// Print the features and their values.
fn print_features(streams: &mut IoStreams) {
    // TODO: move this to features.rs
    let mut max_len = i32::MIN;
    for md in features::METADATA {
        max_len = max_len.max(md.name.len() as i32);
    }
    for md in features::METADATA {
        let set = if feature_test(md.flag) {
            L!("on")
        } else {
            L!("off")
        };
        streams.out.append(sprintf!(
            "%-*ls%-3s %ls %ls\n",
            max_len + 1,
            md.name,
            set,
            md.groups,
            md.description,
        ));
    }
}

fn parse_cmd_opts(
    opts: &mut StatusCmdOpts,
    optind: &mut usize,
    args: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> Option<c_int> {
    let cmd = args[0];

    let mut args_read = Vec::with_capacity(args.len());
    args_read.extend_from_slice(args);

    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, args);
    while let Some(c) = w.next_opt() {
        match c {
            'L' => {
                opts.level = {
                    let arg = w.woptarg.expect("Option -L requires an argument");
                    match fish_wcstoi(arg) {
                        Ok(level) if level >= 0 => level,
                        Err(Error::Overflow) | Ok(_) => {
                            streams.err.append(wgettext_fmt!(
                                "%ls: Invalid level value '%ls'\n",
                                cmd,
                                arg
                            ));
                            return STATUS_INVALID_ARGS;
                        }
                        _ => {
                            streams
                                .err
                                .append(wgettext_fmt!(BUILTIN_ERR_NOT_NUMBER, cmd, arg));
                            return STATUS_INVALID_ARGS;
                        }
                    }
                };
            }
            flag @ ('c' | 'b' | 'i' | 'l' | 'f' | 'n' | 't') => {
                let subcmd = match flag {
                    'c' => STATUS_IS_COMMAND_SUB,
                    'b' => STATUS_IS_BLOCK,
                    'i' => STATUS_IS_INTERACTIVE,
                    'l' => STATUS_IS_LOGIN,
                    'f' => STATUS_FILENAME,
                    'n' => STATUS_LINE_NUMBER,
                    't' => STATUS_STACK_TRACE,
                    _ => unreachable!(),
                };
                if !opts.try_set_status_cmd(subcmd, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            'j' => {
                if !opts.try_set_status_cmd(STATUS_SET_JOB_CONTROL, streams) {
                    return STATUS_CMD_ERROR;
                }
                let Ok(job_mode) = w.woptarg.unwrap().try_into() else {
                    streams.err.append(wgettext_fmt!(
                        "%ls: Invalid job control mode '%ls'\n",
                        cmd,
                        w.woptarg.unwrap()
                    ));
                    return STATUS_CMD_ERROR;
                };
                opts.new_job_control_mode = Some(job_mode);
            }
            IS_FULL_JOB_CTRL_SHORT => {
                if !opts.try_set_status_cmd(STATUS_IS_FULL_JOB_CTRL, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            IS_INTERACTIVE_JOB_CTRL_SHORT => {
                if !opts.try_set_status_cmd(STATUS_IS_INTERACTIVE_JOB_CTRL, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            IS_NO_JOB_CTRL_SHORT => {
                if !opts.try_set_status_cmd(STATUS_IS_NO_JOB_CTRL, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            FISH_PATH_SHORT => {
                if !opts.try_set_status_cmd(STATUS_FISH_PATH, streams) {
                    return STATUS_CMD_ERROR;
                }
            }
            'h' => opts.print_help = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.wopt_index - 1], false);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    *optind = w.wopt_index;

    return STATUS_CMD_OK;
}

pub fn status(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let cmd = args[0];
    let argc = args.len();

    let mut opts = StatusCmdOpts::default();
    let mut optind = 0usize;
    let retval = parse_cmd_opts(&mut opts, &mut optind, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // If a status command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    if optind < argc {
        match StatusCmd::from_wstr(args[optind].to_string().as_str()) {
            Some(s) => {
                if !opts.try_set_status_cmd(s, streams) {
                    return STATUS_CMD_ERROR;
                }
                optind += 1;
            }
            None => {
                streams
                    .err
                    .append(wgettext_fmt!(BUILTIN_ERR_INVALID_SUBCMD, cmd, args[1]));
                return STATUS_INVALID_ARGS;
            }
        }
    }
    // Every argument that we haven't consumed already is an argument for a subcommand.
    let args = &args[optind..];

    let Some(subcmd) = opts.status_cmd else {
        debug_assert!(args.is_empty(), "passed arguments to nothing");

        if get_login() {
            streams.out.append(wgettext!("This is a login shell\n"));
        } else {
            streams.out.append(wgettext!("This is not a login shell\n"));
        }
        let job_control_mode = match get_job_control_mode() {
            JobControl::interactive => wgettext!("Only on interactive jobs"),
            JobControl::none => wgettext!("Never"),
            JobControl::all => wgettext!("Always"),
        };
        streams
            .out
            .append(wgettext_fmt!("Job control: %ls\n", job_control_mode));
        streams.out.append(parser.stack_trace());

        return STATUS_CMD_OK;
    };

    match subcmd {
        c @ STATUS_SET_JOB_CONTROL => {
            let job_control_mode = match opts.new_job_control_mode {
                Some(j) => {
                    // Flag form used
                    if !args.is_empty() {
                        streams.err.append(wgettext_fmt!(
                            BUILTIN_ERR_ARG_COUNT2,
                            cmd,
                            c.to_wstr(),
                            0,
                            args.len()
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    j
                }
                None => {
                    if args.len() != 1 {
                        streams.err.append(wgettext_fmt!(
                            BUILTIN_ERR_ARG_COUNT2,
                            cmd,
                            c.to_wstr(),
                            1,
                            args.len()
                        ));
                        return STATUS_INVALID_ARGS;
                    }
                    let Ok(new_mode) = args[0].try_into() else {
                        streams.err.append(wgettext_fmt!(
                            "%ls: Invalid job control mode '%ls'\n",
                            cmd,
                            args[0]
                        ));
                        return STATUS_CMD_ERROR;
                    };
                    new_mode
                }
            };
            set_job_control_mode(job_control_mode);
        }
        STATUS_FEATURES => print_features(streams),
        c @ STATUS_TEST_FEATURE => {
            if args.len() != 1 {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_ARG_COUNT2,
                    cmd,
                    c.to_wstr(),
                    1,
                    args.len()
                ));
                return STATUS_INVALID_ARGS;
            }
            use TestFeatureRetVal::*;
            let mut retval = Some(TEST_FEATURE_NOT_RECOGNIZED as c_int);
            for md in features::METADATA {
                if md.name == args[0] {
                    retval = match feature_test(md.flag) {
                        true => Some(TEST_FEATURE_ON as c_int),
                        false => Some(TEST_FEATURE_OFF as c_int),
                    };
                }
            }
            return retval;
        }
        ref s => {
            if !args.is_empty() {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_ARG_COUNT2,
                    cmd,
                    s.to_wstr(),
                    0,
                    args.len()
                ));
                return STATUS_INVALID_ARGS;
            }
            match s {
                STATUS_BASENAME | STATUS_DIRNAME | STATUS_FILENAME => {
                    let res = parser.current_filename();
                    let function = res.unwrap_or_default();
                    let f = match (function.is_empty(), s) {
                        (false, STATUS_DIRNAME) => wdirname(&function),
                        (false, STATUS_BASENAME) => wbasename(&function),
                        (true, _) => wgettext!("Standard input"),
                        (false, _) => &function,
                    };
                    streams.out.appendln(f);
                }
                STATUS_FUNCTION => {
                    let f = match parser.get_function_name(opts.level) {
                        Some(f) => f,
                        None => wgettext!("Not a function").to_owned(),
                    };
                    streams.out.appendln(f);
                }
                STATUS_LINE_NUMBER => {
                    // TBD is how to interpret the level argument when fetching the line number.
                    // See issue #4161.
                    // streams.out.append_format(L"%d\n", parser.get_lineno(opts.level));
                    streams
                        .out
                        .appendln(parser.get_lineno().unwrap_or(0).to_wstring());
                }
                STATUS_IS_INTERACTIVE => {
                    if is_interactive_session() {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_COMMAND_SUB => {
                    if parser.libdata().pods.is_subshell {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_BLOCK => {
                    if parser.is_block() {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_BREAKPOINT => {
                    if parser.is_breakpoint() {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_LOGIN => {
                    if get_login() {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_FULL_JOB_CTRL => {
                    if get_job_control_mode() == JobControl::all {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_INTERACTIVE_JOB_CTRL => {
                    if get_job_control_mode() == JobControl::interactive {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_NO_JOB_CTRL => {
                    if get_job_control_mode() == JobControl::none {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_STACK_TRACE => {
                    streams.out.append(parser.stack_trace());
                }
                STATUS_CURRENT_CMD => {
                    let command = &parser.libdata().status_vars.command;
                    if !command.is_empty() {
                        streams.out.append(command);
                    } else {
                        streams.out.appendln(*PROGRAM_NAME.get().unwrap());
                    }
                    streams.out.append_char('\n');
                }
                STATUS_CURRENT_COMMANDLINE => {
                    let commandline = &parser.libdata().status_vars.commandline;
                    streams.out.append(commandline);
                    streams.out.append_char('\n');
                }
                STATUS_FISH_PATH => {
                    let path = get_executable_path("fish");
                    if path.is_empty() {
                        streams.err.append(wgettext_fmt!(
                            "%ls: Could not get executable path: '%s'\n",
                            cmd,
                            Errno::last().to_string()
                        ));
                    }
                    if path.is_absolute() {
                        let path = str2wcstring(path.as_os_str().as_bytes());
                        // This is an absoulte path, we can canonicalize it
                        let real = match wrealpath(&path) {
                            Some(p) if waccess(&p, F_OK) == 0 => p,
                            // realpath did not work, just append the path
                            // - maybe this was obtained via $PATH?
                            _ => path,
                        };

                        streams.out.append(real);
                        streams.out.append_char('\n');
                    } else {
                        // This is a relative path, we can't canonicalize it
                        let path = str2wcstring(path.as_os_str().as_bytes());
                        streams.out.appendln(path);
                    }
                }
                STATUS_SET_JOB_CONTROL | STATUS_FEATURES | STATUS_TEST_FEATURE => {
                    unreachable!("")
                }
            }
        }
    };

    return retval;
}
