use std::os::unix::prelude::*;

use super::prelude::*;
use crate::common::{get_executable_path, str2wcstring, PROGRAM_NAME};
use crate::ffi::{
    get_job_control_mode, get_login, is_interactive_session, job_control_t, set_job_control_mode,
};
use crate::future_feature_flags::{self as features, feature_test};
use crate::wutil::{waccess, wbasename, wdirname, wrealpath, Error};
use libc::F_OK;
use nix::errno::Errno;
use nix::NixPath;
use num_derive::FromPrimitive;
use num_traits::FromPrimitive;

macro_rules! str_enum {
    ($name:ident, $(($val:ident, $str:expr)),* $(,)?) => {
        impl TryFrom<&str> for $name {
            type Error = ();

            fn try_from(s: &str) -> Result<Self, Self::Error> {
                // matching on str's lets us avoid having to do binary search and friends ourselves,
                // this is ascii only anyways
                match s {
                    $($str => Ok(Self::$val)),*,
                    _ => Err(()),
                }
            }
        }

        impl $name {
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
#[derive(PartialEq, FromPrimitive, Clone, Copy)]
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

impl StatusCmd {
    const fn as_char(self) -> char {
        match char::from_u32(self as u32) {
            Some(c) => c,
            None => panic!("Should be a valid char"),
        }
    }
}

/// Values that may be returned from the test-feature option to status.
#[repr(i32)]
enum TestFeatureRetVal {
    TEST_FEATURE_ON = 0,
    TEST_FEATURE_OFF,
    TEST_FEATURE_NOT_RECOGNIZED,
}

struct StatusCmdOpts {
    level: i32,
    new_job_control_mode: Option<job_control_t>,
    status_cmd: Option<StatusCmd>,
    print_help: bool,
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

const SHORT_OPTIONS: &wstr = L!(":L:cbilfnhj:t");
const LONG_OPTIONS: &[woption] = &[
    wopt(L!("help"), no_argument, 'h'),
    wopt(L!("current-filename"), no_argument, 'f'),
    wopt(L!("current-line-number"), no_argument, 'n'),
    wopt(L!("filename"), no_argument, 'f'),
    wopt(L!("fish-path"), no_argument, STATUS_FISH_PATH.as_char()),
    wopt(L!("is-block"), no_argument, 'b'),
    wopt(L!("is-command-substitution"), no_argument, 'c'),
    wopt(
        L!("is-full-job-control"),
        no_argument,
        STATUS_IS_FULL_JOB_CTRL.as_char(),
    ),
    wopt(L!("is-interactive"), no_argument, 'i'),
    wopt(
        L!("is-interactive-job-control"),
        no_argument,
        STATUS_IS_INTERACTIVE_JOB_CTRL.as_char(),
    ),
    wopt(L!("is-login"), no_argument, 'l'),
    wopt(
        L!("is-no-job-control"),
        no_argument,
        STATUS_IS_NO_JOB_CTRL.as_char(),
    ),
    wopt(L!("job-control"), required_argument, 'j'),
    wopt(L!("level"), required_argument, 'L'),
    wopt(L!("line"), no_argument, 'n'),
    wopt(L!("line-number"), no_argument, 'n'),
    wopt(L!("print-stack-trace"), no_argument, 't'),
];

/// Print the features and their values.
fn print_features(streams: &mut io_streams_t) {
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
    parser: &mut parser_t,
    streams: &mut io_streams_t,
) -> Option<c_int> {
    let cmd = args[0];

    let mut args_read = Vec::with_capacity(args.len());
    args_read.extend_from_slice(args);

    let mut w = wgetopter_t::new(SHORT_OPTIONS, LONG_OPTIONS, args);
    while let Some(c) = w.wgetopt_long() {
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
                if let Some(existing) = opts.status_cmd.replace(subcmd) {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_COMBO2_EXCLUSIVE,
                        cmd,
                        existing.to_wstr(),
                        subcmd.to_wstr(),
                    ));
                    return STATUS_CMD_ERROR;
                }
            }
            'j' => {
                let subcmd = STATUS_SET_JOB_CONTROL;
                if let Some(existing) = opts.status_cmd.replace(subcmd) {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_COMBO2_EXCLUSIVE,
                        cmd,
                        existing.to_wstr(),
                        subcmd.to_wstr(),
                    ));
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
            'h' => opts.print_help = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.woptind - 1], false);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.woptind - 1], false);
                return STATUS_INVALID_ARGS;
            }
            c => {
                let Some(opt_cmd) = StatusCmd::from_u32(c.into()) else {
                    panic!("unexpected retval from wgetopt_long")
                };
                match opt_cmd {
                    STATUS_IS_FULL_JOB_CTRL
                    | STATUS_IS_INTERACTIVE_JOB_CTRL
                    | STATUS_IS_NO_JOB_CTRL
                    | STATUS_FISH_PATH => {
                        if let Some(existing) = opts.status_cmd.replace(opt_cmd) {
                            streams.err.append(wgettext_fmt!(
                                BUILTIN_ERR_COMBO2_EXCLUSIVE,
                                cmd,
                                existing.to_wstr(),
                                opt_cmd.to_wstr(),
                            ));
                            return STATUS_CMD_ERROR;
                        }
                    }
                    _ => panic!("unexpected retval from wgetopt_long"),
                }
            }
        }
    }

    *optind = w.woptind;

    return STATUS_CMD_OK;
}

pub fn status(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
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
        match StatusCmd::try_from(args[optind].to_string().as_str()) {
            Ok(s) => {
                if let Some(existing) = opts.status_cmd.replace(s) {
                    streams.err.append(wgettext_fmt!(
                        BUILTIN_ERR_COMBO2_EXCLUSIVE,
                        cmd,
                        existing.to_wstr(),
                        s.to_wstr(),
                    ));
                    return STATUS_CMD_ERROR;
                }
                optind += 1;
            }
            Err(_) => {
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
            job_control_t::interactive => wgettext!("Only on interactive jobs"),
            job_control_t::none => wgettext!("Never"),
            job_control_t::all => wgettext!("Always"),
        };
        streams
            .out
            .append(wgettext_fmt!("Job control: %ls\n", job_control_mode));
        streams.out.append(parser.stack_trace().as_wstr());

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
                    let res = parser.current_filename_ffi().from_ffi();
                    let f = match (res.is_empty(), s) {
                        (false, STATUS_DIRNAME) => wdirname(&res),
                        (false, STATUS_BASENAME) => wbasename(&res),
                        (true, _) => wgettext!("Standard input"),
                        (false, _) => &res,
                    };
                    streams.out.appendln(f);
                }
                STATUS_FUNCTION => {
                    let f = match parser.get_func_name(opts.level) {
                        Some(f) => f,
                        None => wgettext!("Not a function").to_owned(),
                    };
                    streams.out.appendln(f);
                }
                STATUS_LINE_NUMBER => {
                    // TBD is how to interpret the level argument when fetching the line number.
                    // See issue #4161.
                    // streams.out.append_format(L"%d\n", parser.get_lineno(opts.level));
                    streams.out.appendln(parser.get_lineno().0.to_wstring());
                }
                STATUS_IS_INTERACTIVE => {
                    if is_interactive_session() {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_COMMAND_SUB => {
                    if parser.libdata_pod().is_subshell {
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
                    if get_job_control_mode() == job_control_t::all {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_INTERACTIVE_JOB_CTRL => {
                    if get_job_control_mode() == job_control_t::interactive {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_IS_NO_JOB_CTRL => {
                    if get_job_control_mode() == job_control_t::none {
                        return STATUS_CMD_OK;
                    } else {
                        return STATUS_CMD_ERROR;
                    }
                }
                STATUS_STACK_TRACE => {
                    streams.out.append(parser.stack_trace().as_wstr());
                }
                STATUS_CURRENT_CMD => {
                    let var = parser.pin().libdata().get_status_vars_command().from_ffi();
                    if !var.is_empty() {
                        streams.out.appendln(var);
                    } else {
                        streams.out.appendln(*PROGRAM_NAME.get().unwrap());
                    }
                }
                STATUS_CURRENT_COMMANDLINE => {
                    let var = parser
                        .pin()
                        .libdata()
                        .get_status_vars_commandline()
                        .from_ffi();
                    streams.out.appendln(var);
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

                        streams.out.appendln(real);
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
