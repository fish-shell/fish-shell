use std::os::unix::prelude::*;

use super::prelude::*;
use crate::common::{bytes2wcstring, get_program_name};
use crate::env::config_paths::get_fish_path;
use crate::future_feature_flags::{self as features, feature_test};
use crate::proc::{
    JobControl, get_job_control_mode, get_login, is_interactive_session, set_job_control_mode,
};
use crate::reader::reader_in_interactive_read;
use crate::tty_handoff::{TERMINAL_OS_NAME, get_scroll_content_up_capability, xtversion};
use crate::wutil::{Error, waccess, wbasename, wdirname, wrealpath};
use cfg_if::cfg_if;
use libc::F_OK;
use rust_embed::RustEmbed;

/// Create an enum with name `$name`.
/// Its variants are given as comma-separated tuples, with each tuple containing the variant name,
/// as well as a string representations of the variant.
/// Optionally, additional strings can be provided, which are considered aliases for the enum
/// variant.
/// The generated enum will implement a `from_str` function, mapping from `&str` to the associated
/// enum variant if any, and `to_wstr`, which maps from a variant to the associated
/// `$default_name`.
/// It is the user's responsibility to ensure than none of the string arguments appears more than
/// once.
macro_rules! str_enum {
    ($name:ident, $(($val:ident, $default_name:expr $(, $alias:expr)* $(,)?)),* $(,)?) => {
        #[derive(Clone, Copy)]
        enum $name {
            $($val),*,
        }

        impl $name {
            fn from_str(s: &str) -> Option<Self> {
                // matching on str's lets us avoid having to do binary search and friends ourselves,
                // this is ASCII only anyways
                match s {
                    $($default_name => Some(Self::$val)),*,
                    $($($alias => Some(Self::$val),)*)*
                    _ => None,
                }
            }

            fn to_wstr(self) -> &'static wstr {
                match self {
                    $(Self::$val => L!($default_name)),*,
                }
            }
        }
    }
}

use StatusCmd::*;
str_enum!(
    StatusCmd,
    (STATUS_BASENAME, "basename", "current-basename"),
    (STATUS_BUILD_INFO, "build-info", "buildinfo"),
    (STATUS_CURRENT_CMD, "current-command"),
    (STATUS_CURRENT_COMMANDLINE, "current-commandline"),
    (STATUS_DIRNAME, "dirname", "current-dirname"),
    (STATUS_FEATURES, "features"),
    (STATUS_FILENAME, "filename", "current-filename"),
    (STATUS_FISH_PATH, "fish-path"),
    (STATUS_FUNCTION, "function", "current-function"),
    (STATUS_GET_FILE, "get-file"),
    (STATUS_IS_BLOCK, "is-block"),
    (STATUS_IS_BREAKPOINT, "is-breakpoint"),
    (STATUS_IS_COMMAND_SUB, "is-command-substitution"),
    (STATUS_IS_FULL_JOB_CTRL, "is-full-job-control"),
    (STATUS_IS_INTERACTIVE, "is-interactive"),
    (STATUS_IS_INTERACTIVE_JOB_CTRL, "is-interactive-job-control"),
    (STATUS_IS_INTERACTIVE_READ, "is-interactive-read"),
    (STATUS_IS_LOGIN, "is-login"),
    (STATUS_IS_NO_JOB_CTRL, "is-no-job-control"),
    (STATUS_LINE_NUMBER, "line-number", "current-line-number"),
    (STATUS_LIST_FILES, "list-files"),
    (STATUS_SET_JOB_CONTROL, "job-control"),
    (STATUS_STACK_TRACE, "stack-trace", "print-stack-trace"),
    (STATUS_TERMINAL, "terminal"),
    (STATUS_TERMINAL_OS, "terminal-os"),
    (STATUS_TEST_FEATURE, "test-feature"),
    (STATUS_TEST_TERMINAL_FEATURE, "test-terminal-feature"),
);

/// Values that may be returned from the test-feature option to status.
#[repr(i32)]
enum TestFeatureRetVal {
    TEST_FEATURE_ON,
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
const IS_INTERACTIVE_READ_SHORT: char = '\x05';

const SHORT_OPTIONS: &wstr = L!("L:cbilfnhj:t");
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
    wopt(
        L!("is-interactive-read"),
        NoArgument,
        IS_INTERACTIVE_READ_SHORT,
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
            "%-*s%-3s %s %s\n",
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
) -> BuiltinResult {
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
                                "%s: Invalid level value '%s'\n",
                                cmd,
                                arg
                            ));
                            return Err(STATUS_INVALID_ARGS);
                        }
                        _ => {
                            streams
                                .err
                                .append(wgettext_fmt!(BUILTIN_ERR_NOT_NUMBER, cmd, arg));
                            return Err(STATUS_INVALID_ARGS);
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
                    return Err(STATUS_CMD_ERROR);
                }
            }
            'j' => {
                if !opts.try_set_status_cmd(STATUS_SET_JOB_CONTROL, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
                let Ok(job_mode) = w.woptarg.unwrap().try_into() else {
                    streams.err.append(wgettext_fmt!(
                        "%s: Invalid job control mode '%s'\n",
                        cmd,
                        w.woptarg.unwrap()
                    ));
                    return Err(STATUS_CMD_ERROR);
                };
                opts.new_job_control_mode = Some(job_mode);
            }
            IS_FULL_JOB_CTRL_SHORT => {
                if !opts.try_set_status_cmd(STATUS_IS_FULL_JOB_CTRL, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            IS_INTERACTIVE_JOB_CTRL_SHORT => {
                if !opts.try_set_status_cmd(STATUS_IS_INTERACTIVE_JOB_CTRL, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            IS_INTERACTIVE_READ_SHORT => {
                if !opts.try_set_status_cmd(STATUS_IS_INTERACTIVE_READ, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            IS_NO_JOB_CTRL_SHORT => {
                if !opts.try_set_status_cmd(STATUS_IS_NO_JOB_CTRL, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            FISH_PATH_SHORT => {
                if !opts.try_set_status_cmd(STATUS_FISH_PATH, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
            }
            'h' => opts.print_help = true,
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    *optind = w.wopt_index;

    return Ok(SUCCESS);
}

struct EmptyEmbed;
impl RustEmbed for EmptyEmbed {
    fn get(_file_path: &str) -> Option<rust_embed::EmbeddedFile> {
        None
    }
    fn iter() -> rust_embed::Filenames {
        use rust_embed::Filenames::*;
        cfg_if!(
            // TODO This is a clone of rebuild_if_embedded_path_changed.
            if #[cfg(any(not(debug_assertions), windows))] {
                let nothing = Embedded([].iter());
            } else {
                let nothing = Dynamic(Box::new(None.into_iter()));
            }
        );
        nothing
    }
}

cfg_if!(
    if #[cfg(feature = "embed-manpages")] {
        #[derive(RustEmbed)]
        #[folder = "$FISH_RESOLVED_BUILD_DIR/fish-man/man1"]
        #[prefix = "man/man1/"]
        struct Docs;
    } else {
        type Docs = EmptyEmbed;
    }
);

cfg_if!(
    if #[cfg(using_cmake)] {
        #[derive(RustEmbed)]
        #[folder = "$FISH_CMAKE_BINARY_DIR/share"]
        #[include = "__fish_build_paths.fish"]
        struct CMakeBinaryDir;
    } else {
        type CMakeBinaryDir = EmptyEmbed;
    }
);

pub fn status(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let cmd = args[0];
    let argc = args.len();

    let mut opts = StatusCmdOpts::default();
    let mut optind = 0usize;
    parse_cmd_opts(&mut opts, &mut optind, args, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    // If a status command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    if optind < argc {
        match StatusCmd::from_str(args[optind].to_string().as_str()) {
            Some(s) => {
                if !opts.try_set_status_cmd(s, streams) {
                    return Err(STATUS_CMD_ERROR);
                }
                optind += 1;
            }
            None => {
                streams
                    .err
                    .append(wgettext_fmt!(BUILTIN_ERR_INVALID_SUBCMD, cmd, args[1]));
                return Err(STATUS_INVALID_ARGS);
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
            JobControl::Interactive => wgettext!("Only on interactive jobs"),
            JobControl::None => wgettext!("Never"),
            JobControl::All => wgettext!("Always"),
        };
        streams
            .out
            .append(wgettext_fmt!("Job control: %s\n", job_control_mode));
        streams.out.append(parser.stack_trace());

        return Ok(SUCCESS);
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
                        return Err(STATUS_INVALID_ARGS);
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
                        return Err(STATUS_INVALID_ARGS);
                    }
                    let Ok(new_mode) = args[0].try_into() else {
                        streams.err.append(wgettext_fmt!(
                            "%s: Invalid job control mode '%s'\n",
                            cmd,
                            args[0]
                        ));
                        return Err(STATUS_CMD_ERROR);
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
                return Err(STATUS_INVALID_ARGS);
            }
            let mut retval = TestFeatureRetVal::TEST_FEATURE_NOT_RECOGNIZED;
            for md in features::METADATA {
                if md.name == args[0] {
                    retval = match feature_test(md.flag) {
                        true => TestFeatureRetVal::TEST_FEATURE_ON,
                        false => TestFeatureRetVal::TEST_FEATURE_OFF,
                    };
                }
            }
            return Err(retval as i32);
        }
        c @ STATUS_GET_FILE => {
            if args.len() != 1 {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_ARG_COUNT2,
                    cmd,
                    c.to_wstr(),
                    1,
                    args.len()
                ));
                return Err(STATUS_INVALID_ARGS);
            }
            let arg = crate::common::wcs2bytes(args[0]);
            let arg = std::str::from_utf8(&arg).unwrap();
            let Some(emfile) = crate::autoload::Asset::get(arg)
                .or_else(|| Docs::get(arg))
                .or_else(|| CMakeBinaryDir::get(arg))
            else {
                return Err(STATUS_CMD_ERROR);
            };
            let src = bytes2wcstring(&emfile.data);
            streams.out.append(src);
            return Ok(SUCCESS);
        }
        STATUS_LIST_FILES => {
            use crate::util::wcsfilecmp_glob;
            let mut paths = vec![];
            let mut add = |arg| {
                let arg = crate::common::wcs2bytes(arg);
                let arg = std::str::from_utf8(&arg).unwrap();
                for path in crate::autoload::Asset::iter()
                    .chain(Docs::iter())
                    .chain(CMakeBinaryDir::iter())
                {
                    if arg.is_empty() || path.starts_with(arg) {
                        paths.push(bytes2wcstring(path.as_bytes()));
                    }
                }
            };
            if args.is_empty() {
                add(L!(""));
            } else {
                for arg in args {
                    add(arg);
                }
            }

            paths.sort_by(|a, b| wcsfilecmp_glob(a, b));
            for path in &paths {
                streams.out.appendln(path);
            }

            if !paths.is_empty() {
                return Ok(SUCCESS);
            } else {
                return Err(STATUS_CMD_ERROR);
            }
        }
        c @ STATUS_TEST_TERMINAL_FEATURE => {
            if args.len() != 1 {
                streams.err.append(wgettext_fmt!(
                    BUILTIN_ERR_ARG_COUNT2,
                    cmd,
                    c.to_wstr(),
                    1,
                    args.len()
                ));
                return Err(STATUS_INVALID_ARGS);
            }
            if args[0] != "scroll-content-up" {
                streams.err.appendln(wgettext_fmt!(
                    "%s %s: unrecognized feature '%s'",
                    cmd,
                    c.to_wstr(),
                    args[0]
                ));
                return Err(STATUS_INVALID_ARGS);
            };
            return if get_scroll_content_up_capability() == Some(true) {
                Ok(SUCCESS)
            } else {
                Err(STATUS_CMD_ERROR)
            };
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
                return Err(STATUS_INVALID_ARGS);
            }
            match s {
                STATUS_BUILD_INFO => {
                    let version = bytes2wcstring(crate::BUILD_VERSION.as_bytes());
                    let target = bytes2wcstring(env!("BUILD_TARGET_TRIPLE").as_bytes());
                    let host = bytes2wcstring(env!("BUILD_HOST_TRIPLE").as_bytes());
                    let profile = bytes2wcstring(env!("BUILD_PROFILE").as_bytes());
                    streams.out.append(L!("Build system: "));
                    let buildsystem = if cfg!(using_cmake) { "CMake" } else { "Cargo" };
                    streams.out.appendln(bytes2wcstring(buildsystem.as_bytes()));
                    streams.out.append(L!("Version: "));
                    streams.out.appendln(version);
                    if target == host {
                        streams.out.append(L!("Target (and host): "));
                        streams.out.appendln(target);
                    } else {
                        streams.out.append(L!("Target: "));
                        streams.out.appendln(target);
                        streams.out.append(L!("Host: "));
                        streams.out.appendln(host);
                    }
                    streams.out.append(L!("Profile: "));
                    streams.out.appendln(profile);
                    streams.out.append(L!("Features: "));
                    let features: &[&str] = &[
                        #[cfg(feature = "embed-manpages")]
                        "embed-manpages",
                        #[cfg(feature = "localize-messages")]
                        "localize-messages",
                        #[cfg(target_feature = "crt-static")]
                        "crt-static",
                    ];
                    streams
                        .out
                        .appendln(bytes2wcstring(features.join(" ").as_bytes()));
                    streams.out.appendln("");
                    return Ok(SUCCESS);
                }
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
                        .appendln(parser.get_lineno_for_display().to_wstring());
                }
                STATUS_IS_INTERACTIVE => {
                    if is_interactive_session() {
                        return Ok(SUCCESS);
                    }
                    return Err(STATUS_CMD_ERROR);
                }
                STATUS_IS_COMMAND_SUB => {
                    if parser.scope().is_subshell {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_IS_BLOCK => {
                    if parser.is_block() {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_IS_BREAKPOINT => {
                    if parser.is_breakpoint() {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_IS_LOGIN => {
                    if get_login() {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_IS_FULL_JOB_CTRL => {
                    if get_job_control_mode() == JobControl::All {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_IS_INTERACTIVE_JOB_CTRL => {
                    if get_job_control_mode() == JobControl::Interactive {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_IS_INTERACTIVE_READ => {
                    if reader_in_interactive_read() {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_IS_NO_JOB_CTRL => {
                    if get_job_control_mode() == JobControl::None {
                        return Ok(SUCCESS);
                    } else {
                        return Err(STATUS_CMD_ERROR);
                    }
                }
                STATUS_STACK_TRACE => {
                    streams.out.append(parser.stack_trace());
                }
                STATUS_CURRENT_CMD => {
                    let command = &parser.libdata().status_vars.command;
                    if !command.is_empty() {
                        streams.out.appendln(command);
                    } else {
                        streams.out.appendln(get_program_name());
                    }
                }
                STATUS_CURRENT_COMMANDLINE => {
                    let commandline = &parser.libdata().status_vars.commandline;
                    streams.out.append(commandline);
                    streams.out.append_char('\n');
                }
                STATUS_FISH_PATH => {
                    use crate::env::config_paths::FishPath::*;
                    let result = match get_fish_path() {
                        Absolute(path) => {
                            let path = bytes2wcstring(path.as_os_str().as_bytes());
                            Cow::Owned(match wrealpath(&path) {
                                Some(p) if waccess(&p, F_OK) == 0 => p,
                                // realpath did not work, just append the path
                                // - maybe this was obtained via $PATH?
                                _ => path,
                            })
                        }
                        LookUpInPath => Cow::Borrowed(get_program_name()),
                    };
                    streams.out.appendln(result);
                }
                STATUS_TERMINAL => {
                    let xtversion = xtversion().unwrap_or_default();
                    streams.out.appendln(xtversion);
                }
                STATUS_TERMINAL_OS => {
                    let Some(Some(terminal_os_name)) = TERMINAL_OS_NAME.get() else {
                        return Err(STATUS_CMD_ERROR);
                    };
                    streams.out.appendln(first_line(terminal_os_name));
                }
                STATUS_SET_JOB_CONTROL
                | STATUS_FEATURES
                | STATUS_TEST_FEATURE
                | STATUS_GET_FILE
                | STATUS_LIST_FILES
                | STATUS_TEST_TERMINAL_FEATURE => {
                    unreachable!("")
                }
            }
        }
    };

    Ok(SUCCESS)
}

fn first_line(s: &wstr) -> &wstr {
    &s[..s.chars().position(|c| c == '\n').unwrap_or(s.len())]
}
