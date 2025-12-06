//
// The main loop of the fish program.
/*
Copyright (C) 2005-2008 Axel Liljencrantz

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

use fish::{
    ast,
    builtins::{
        fish_indent, fish_key_reader,
        shared::{
            BUILTIN_ERR_MISSING, BUILTIN_ERR_UNEXP_ARG, BUILTIN_ERR_UNKNOWN, STATUS_CMD_ERROR,
            STATUS_CMD_OK, STATUS_CMD_UNKNOWN,
        },
    },
    common::{
        PACKAGE_NAME, PROFILING_ACTIVE, PROGRAM_NAME, bytes2wcstring, escape,
        save_term_foreground_process_group, wcs2bytes,
    },
    env::{
        EnvMode, Statuses,
        config_paths::ConfigPaths,
        environment::{EnvStack, Environment, env_init},
    },
    eprintf,
    event::{self, Event},
    flog::{self, FLOG, FLOGF, activate_flog_categories_by_pattern, set_flog_file_fd},
    fprintf, function, future_feature_flags as features,
    history::{self, start_private_mode},
    io::IoChain,
    locale::set_libc_locales,
    nix::{RUsage, getpid, getrusage, isatty},
    panic::panic_handler,
    parse_constants::{ParseErrorList, ParseTreeFlags},
    parse_tree::ParsedSource,
    parse_util::parse_util_detect_errors_in_ast,
    parser::{BlockType, CancelBehavior, Parser},
    path::path_get_config,
    printf,
    proc::{
        Pid, get_login, is_interactive_session, mark_login, mark_no_exec, proc_init,
        set_interactive_session,
    },
    reader::{reader_init, reader_read, term_copy_modes},
    signal::{signal_clear_cancel, signal_unblock_all},
    threads::{self},
    topic_monitor,
    wchar::prelude::*,
    wutil::waccess,
};
use libc::STDIN_FILENO;
use std::ffi::{OsStr, OsString};
use std::fs::File;
use std::os::unix::prelude::*;
use std::path::Path;
use std::sync::Arc;
use std::sync::atomic::Ordering;
use std::{env, ops::ControlFlow};

/// container to hold the options specified within the command line
#[derive(Default, Debug)]
struct FishCmdOpts {
    /// Future feature flags values string
    features: WString,
    /// File path for debug output.
    debug_output: Option<OsString>,
    /// File path for profiling output, or empty for none.
    profile_output: Option<OsString>,
    profile_startup_output: Option<OsString>,
    /// Commands to be executed in place of interactive shell.
    batch_cmds: Vec<OsString>,
    /// Commands to execute after the shell's config has been read.
    postconfig_cmds: Vec<OsString>,
    /// Whether to print rusage-self stats after execution.
    print_rusage_self: bool,
    /// Whether no-config is set.
    no_config: bool,
    /// Whether no-exec is set.
    no_exec: bool,
    /// Whether this is a login shell.
    is_login: bool,
    /// Whether this is an interactive session.
    is_interactive_session: bool,
    /// Whether to enable private mode.
    enable_private_mode: bool,
}

/// Return a timeval converted to milliseconds.
#[allow(clippy::unnecessary_cast)]
fn tv_to_msec(tv: &libc::timeval) -> i64 {
    // milliseconds per second
    let mut msec = tv.tv_sec as i64 * 1000;
    // microseconds per millisecond
    msec += tv.tv_usec as i64 / 1000;
    msec
}

fn print_rusage_self() {
    let rs = getrusage(RUsage::RSelf);
    let rss_kb = if cfg!(apple) {
        // mac use bytes.
        rs.ru_maxrss / 1024
    } else {
        // Everyone else uses KB.
        rs.ru_maxrss
    };

    let user_time = tv_to_msec(&rs.ru_utime);
    let sys_time = tv_to_msec(&rs.ru_stime);
    let total_time = user_time + sys_time;
    let signals = rs.ru_nsignals;

    eprintf!("  rusage self:\n");
    eprintf!("      user time: %s ms\n", sys_time.to_string());
    eprintf!("       sys time: %s ms\n", user_time.to_string());
    eprintf!("     total time: %s ms\n", total_time.to_string());
    eprintf!("        max rss: %s kb\n", rss_kb.to_string());
    eprintf!("        signals: %s\n", signals.to_string());
}

// Source the file config.fish in the given directory.
// Returns true if successful, false if not.
fn source_config_in_directory(parser: &Parser, dir: &wstr) -> bool {
    // If the config.fish file doesn't exist or isn't readable silently return. Fish versions up
    // thru 2.2.0 would instead try to source the file with stderr redirected to /dev/null to deal
    // with that possibility.
    //
    // This introduces a race condition since the readability of the file can change between this
    // test and the execution of the 'source' command. However, that is not a security problem in
    // this context so we ignore it.
    let config_pathname = dir.to_owned() + L!("/config.fish");
    let escaped_pathname = escape(dir) + L!("/config.fish");
    if waccess(&config_pathname, libc::R_OK) != 0 {
        FLOGF!(
            config,
            "not sourcing %s (not readable or does not exist)",
            escaped_pathname
        );
        return false;
    }
    FLOG!(config, "sourcing", escaped_pathname);

    let cmd: WString = L!("builtin source ").to_owned() + escaped_pathname.as_utfstr();

    parser.libdata_mut().within_fish_init = true;
    let _ = parser.eval(&cmd, &IoChain::new());
    parser.libdata_mut().within_fish_init = false;
    true
}

/// Parse init files. exec_path is the path of fish executable as determined by argv[0].
fn read_init(parser: &Parser, paths: &ConfigPaths) {
    use fish::autoload::Asset;
    let emfile = Asset::get("config.fish").expect("Embedded file not found");
    let src = bytes2wcstring(&emfile.data);
    parser.libdata_mut().within_fish_init = true;
    let fname: Arc<WString> = Arc::new(L!("embedded:config.fish").into());
    let ret = parser.eval_file_wstr(src, fname, &IoChain::new(), None);
    parser.libdata_mut().within_fish_init = false;
    if let Err(msg) = ret {
        eprintf!("%s", msg);
    }

    source_config_in_directory(
        parser,
        &bytes2wcstring(paths.sysconf.as_os_str().as_bytes()),
    );

    // We need to get the configuration directory before we can source the user configuration file.
    // If path_get_config returns false then we have no configuration directory and no custom config
    // to load.
    if let Some(config_dir) = path_get_config() {
        source_config_in_directory(parser, &config_dir);
    }
}

fn run_command_list(parser: &Parser, cmds: &[OsString]) -> Result<(), libc::c_int> {
    let mut retval = Ok(());
    for cmd in cmds {
        let cmd_wcs = bytes2wcstring(cmd.as_bytes());

        let mut errors = ParseErrorList::new();
        let ast = ast::parse(&cmd_wcs, ParseTreeFlags::empty(), Some(&mut errors));
        let errored = ast.errored() || {
            parse_util_detect_errors_in_ast(&ast, &cmd_wcs, Some(&mut errors)).is_err()
        };

        if !errored {
            // Construct a parsed source ref.
            let ps = Arc::new(ParsedSource::new(cmd_wcs, ast));
            let _ = parser.eval_parsed_source(&ps, &IoChain::new(), None, BlockType::top, false);
            retval = Ok(());
        } else {
            let backtrace = parser.get_backtrace(&cmd_wcs, &errors);
            eprintf!("%s", backtrace);
            // XXX: Why is this the return for "unknown command"?
            retval = Err(STATUS_CMD_UNKNOWN);
        }
    }

    retval
}

fn fish_parse_opt(args: &mut [WString], opts: &mut FishCmdOpts) -> ControlFlow<i32, usize> {
    use fish::wgetopt::{ArgType::*, WGetopter, WOption, wopt};

    const RUSAGE_ARG: char = 1 as char;
    const PRINT_DEBUG_CATEGORIES_ARG: char = 2 as char;
    const PROFILE_STARTUP_ARG: char = 3 as char;

    const SHORT_OPTS: &wstr = L!("+hPilNnvc:C:p:d:f:D:o:");
    const LONG_OPTS: &[WOption<'static>] = &[
        wopt(L!("command"), RequiredArgument, 'c'),
        wopt(L!("init-command"), RequiredArgument, 'C'),
        wopt(L!("features"), RequiredArgument, 'f'),
        wopt(L!("debug"), RequiredArgument, 'd'),
        wopt(L!("debug-output"), RequiredArgument, 'o'),
        wopt(L!("debug-stack-frames"), RequiredArgument, 'D'),
        wopt(L!("interactive"), NoArgument, 'i'),
        wopt(L!("login"), NoArgument, 'l'),
        wopt(L!("no-config"), NoArgument, 'N'),
        wopt(L!("no-execute"), NoArgument, 'n'),
        wopt(L!("print-rusage-self"), NoArgument, RUSAGE_ARG),
        wopt(
            L!("print-debug-categories"),
            NoArgument,
            PRINT_DEBUG_CATEGORIES_ARG,
        ),
        wopt(L!("profile"), RequiredArgument, 'p'),
        wopt(L!("profile-startup"), RequiredArgument, PROFILE_STARTUP_ARG),
        wopt(L!("private"), NoArgument, 'P'),
        wopt(L!("help"), NoArgument, 'h'),
        wopt(L!("version"), NoArgument, 'v'),
    ];

    let mut shim_args: Vec<&wstr> = args.iter().map(|s| s.as_ref()).collect();
    let mut w = WGetopter::new(SHORT_OPTS, LONG_OPTS, &mut shim_args);
    while let Some(c) = w.next_opt() {
        match c {
            'c' => opts
                .batch_cmds
                .push(OsString::from_vec(wcs2bytes(w.woptarg.unwrap()))),
            'C' => opts
                .postconfig_cmds
                .push(OsString::from_vec(wcs2bytes(w.woptarg.unwrap()))),
            'd' => {
                activate_flog_categories_by_pattern(w.woptarg.unwrap());
                for cat in flog::categories::all_categories() {
                    if cat.enabled.load(Ordering::Relaxed) {
                        printf!("Debug enabled for category: %s\n", cat.name);
                    }
                }
            }
            'o' => opts.debug_output = Some(OsString::from_vec(wcs2bytes(w.woptarg.unwrap()))),
            'f' => opts.features = w.woptarg.unwrap().to_owned(),
            'h' => opts.batch_cmds.push("__fish_print_help fish".into()),
            'i' => opts.is_interactive_session = true,
            'l' => opts.is_login = true,
            'N' => {
                opts.no_config = true;
                // --no-config implies private mode, we won't be saving history
                opts.enable_private_mode = true;
            }
            'n' => opts.no_exec = true,
            RUSAGE_ARG => opts.print_rusage_self = true,
            PRINT_DEBUG_CATEGORIES_ARG => {
                let cats = flog::categories::all_categories();
                // Compute width of longest name.
                let mut name_width = 0;
                for cat in cats.iter() {
                    name_width = usize::max(name_width, cat.name.len());
                }
                // A little extra space.
                name_width += 2;
                for cat in cats.iter() {
                    let desc = cat.description.localize();
                    // this is left-justified
                    printf!("%-*s %s\n", name_width, cat.name, desc);
                }
                return ControlFlow::Break(0);
            }
            // "--profile" - this does not activate profiling right away,
            // rather it's done after startup is finished.
            'p' => opts.profile_output = Some(OsString::from_vec(wcs2bytes(w.woptarg.unwrap()))),
            PROFILE_STARTUP_ARG => {
                // With "--profile-startup" we immediately turn profiling on.
                opts.profile_startup_output =
                    Some(OsString::from_vec(wcs2bytes(w.woptarg.unwrap())));
                PROFILING_ACTIVE.store(true);
            }
            'P' => opts.enable_private_mode = true,
            'v' => {
                printf!(
                    "%s",
                    wgettext_fmt!("%s, version %s\n", PACKAGE_NAME, fish::BUILD_VERSION)
                );
                return ControlFlow::Break(0);
            }
            'D' => {
                // TODO: Option is currently useless.
                // Either remove it or make it work with FLOG.
            }
            '?' => {
                eprintf!(
                    "%s\n",
                    wgettext_fmt!(BUILTIN_ERR_UNKNOWN, "fish", args[w.wopt_index - 1])
                );
                return ControlFlow::Break(1);
            }
            ':' => {
                eprintf!(
                    "%s\n",
                    wgettext_fmt!(BUILTIN_ERR_MISSING, "fish", args[w.wopt_index - 1])
                );
                return ControlFlow::Break(1);
            }
            ';' => {
                eprintf!(
                    "%s\n",
                    wgettext_fmt!(BUILTIN_ERR_UNEXP_ARG, "fish", args[w.wopt_index - 1])
                );
                return ControlFlow::Break(1);
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }
    let optind = w.wopt_index;

    // If our command name begins with a dash that implies we're a login shell.
    opts.is_login |= args[0].char_at(0) == '-';

    // We are an interactive session if we have not been given an explicit
    // command or file to execute and stdin is a tty. Note that the -i or
    // --interactive options also force interactive mode.
    if opts.batch_cmds.is_empty() && optind == args.len() && isatty(STDIN_FILENO) {
        set_interactive_session(true);
    }

    ControlFlow::Continue(optind)
}

fn main() {
    // If we are called as "/path/to/fish_key_reader", become fish_key_reader.
    if let Some(name) = env::args_os().next() {
        let p = Path::new(&name).file_name().and_then(|x| x.to_str());
        if p == Some("fish_key_reader") {
            return fish_key_reader::main();
        } else if p == Some("fish_indent") {
            return fish_indent::main();
        }
    }
    PROGRAM_NAME.set(L!("fish")).unwrap();
    if !cfg!(small_main_stack) {
        panic_handler(throwing_main);
    } else {
        // Create a new thread with a decent stack size to be our main thread
        std::thread::scope(|scope| {
            scope.spawn(|| panic_handler(throwing_main));
        })
    }
}

fn throwing_main() -> i32 {
    let mut res = Err(STATUS_CMD_ERROR);

    signal_unblock_all();
    topic_monitor::topic_monitor_init();
    threads::init();

    // Safety: single-threaded.
    unsafe {
        set_libc_locales(/*log_ok=*/ false)
    };

    fish::wutil::gettext::initialize_gettext();

    // Enable debug categories set in FISH_DEBUG.
    // This is in *addition* to the ones given via --debug.
    if let Some(debug_categories) = env::var_os("FISH_DEBUG") {
        let s = bytes2wcstring(debug_categories.as_bytes());
        activate_flog_categories_by_pattern(&s);
    }

    let mut args: Vec<WString> = env::args_os()
        .map(|osstr| bytes2wcstring(osstr.as_bytes()))
        .collect();
    let mut opts = FishCmdOpts::default();
    let mut my_optind = match fish_parse_opt(&mut args, &mut opts) {
        ControlFlow::Continue(optind) => optind,
        ControlFlow::Break(status) => return status,
    };

    // Direct any debug output right away.
    // --debug-output takes precedence, otherwise $FISH_DEBUG_OUTPUT is used.
    // PORTING: this is a slight difference from C++, we now skip reading the env var if the argument is an empty string
    if opts.debug_output.is_none() {
        opts.debug_output = env::var_os("FISH_DEBUG_OUTPUT");
    }

    if let Some(debug_path) = opts.debug_output {
        match File::options()
            .write(true)
            .truncate(true)
            .create(true)
            .open(&debug_path)
        {
            Ok(dbg_file) => {
                // Rust sets O_CLOEXEC by default
                // https://github.com/rust-lang/rust/blob/07438b0928c6691d6ee734a5a77823ec143be94d/library/std/src/sys/unix/fs.rs#L1059
                set_flog_file_fd(dbg_file.into_raw_fd());
            }
            Err(e) => {
                let debug_path_string = format!("{debug_path:?}");
                // TODO: should be translated
                eprintf!("Could not open file %s\n", debug_path_string);
                eprintf!("%s\n", e);
                return 1;
            }
        };
    }

    // No-exec is prohibited when in interactive mode.
    if opts.is_interactive_session && opts.no_exec {
        FLOG!(
            warning,
            wgettext!("Can not use the no-execute mode when running an interactive session")
        );
        opts.no_exec = false;
    }

    // Apply our options
    if opts.is_login {
        mark_login();
    }
    if opts.no_exec {
        mark_no_exec();
    }
    if opts.is_interactive_session {
        set_interactive_session(true);
    }
    if opts.enable_private_mode {
        start_private_mode(EnvStack::globals());
    }

    // Only save (and therefore restore) the fg process group if we are interactive. See issues
    // #197 and #1002.
    if is_interactive_session() {
        save_term_foreground_process_group();
    }

    // If we're not executing, there's no need to find the config.
    let config_paths = if !opts.no_exec {
        let config_paths = ConfigPaths::new();
        env_init(
            Some(&config_paths),
            /* do uvars */ !opts.no_config,
            /* default paths */ opts.no_config,
        );
        Some(config_paths)
    } else {
        None
    };

    // Set features early in case other initialization depends on them.
    // Start with the ones set in the environment, then those set on the command line (so the
    // command line takes precedence).
    if let Some(features_var) = EnvStack::globals().get(L!("fish_features")) {
        for s in features_var.as_list() {
            features::set_from_string(s.as_utfstr());
        }
    }
    features::set_from_string(opts.features.as_utfstr());
    fish::env_dispatch::read_terminfo_database(EnvStack::globals());
    proc_init();
    reader_init(true);

    // Construct the root parser!
    let env = EnvStack::globals().create_child(true /* dispatches_var_changes */);
    let parser = &Parser::new(env, CancelBehavior::Clear);
    parser.set_syncs_uvars(!opts.no_config);

    if !opts.no_exec && !opts.no_config {
        read_init(parser, config_paths.as_ref().unwrap());
    }

    if is_interactive_session() && opts.no_config && !opts.no_exec {
        // If we have no config, we default to the default key bindings.
        parser.vars().set_one(
            L!("fish_key_bindings"),
            EnvMode::UNEXPORT,
            L!("fish_default_key_bindings").to_owned(),
        );
        if function::exists(L!("fish_default_key_bindings"), parser) {
            let _ = run_command_list(parser, &[OsString::from("fish_default_key_bindings")]);
        }
    }

    // Re-read the terminal modes after config, it might have changed them.
    term_copy_modes();

    // Stomp the exit status of any initialization commands (issue #635).
    parser.set_last_statuses(Statuses::just(STATUS_CMD_OK));

    // TODO: if-let-chains
    if let Some(path) = &opts.profile_startup_output {
        if opts.profile_startup_output != opts.profile_output {
            parser.emit_profiling(path);

            // If we are profiling both, ensure the startup data only
            // ends up in the startup file.
            parser.clear_profiling();
        }
    }

    PROFILING_ACTIVE.store(opts.profile_output.is_some());

    // Run post-config commands specified as arguments, if any.
    if !opts.postconfig_cmds.is_empty() {
        res = run_command_list(parser, &opts.postconfig_cmds);
    }

    // Clear signals in case we were interrupted (#9024).
    signal_clear_cancel();

    if !opts.batch_cmds.is_empty() {
        // Run the commands specified as arguments, if any.
        if get_login() {
            // Do something nasty to support OpenSUSE assuming we're bash. This may modify cmds.
            fish_xdm_login_hack_hack_hack_hack(&mut opts.batch_cmds, &args[my_optind..]);
        }

        // Pass additional args as $argv.
        // Note that we *don't* support setting argv[0]/$0, unlike e.g. bash.
        let list = &args[my_optind..];
        parser.vars().set(
            L!("argv"),
            EnvMode::default(),
            list.iter().map(|s| s.to_owned()).collect(),
        );
        res = run_command_list(parser, &opts.batch_cmds);
        parser.libdata_mut().exit_current_script = false;
    } else if my_optind == args.len() {
        // Implicitly interactive mode.
        if opts.no_exec && isatty(libc::STDIN_FILENO) {
            FLOG!(
                error,
                "no-execute mode enabled and no script given. Exiting"
            );
            // above line should always exit
            return libc::EXIT_FAILURE;
        }
        res = reader_read(parser, libc::STDIN_FILENO, &IoChain::new());
    } else {
        let n = wcs2bytes(&args[my_optind]);
        let path = OsStr::from_bytes(&n);
        my_optind += 1;
        // Rust sets cloexec by default, see above
        // We don't need autoclose_fd_t when we use File, it will be closed on drop.
        match File::open(path) {
            Err(e) => {
                FLOGF!(
                    error,
                    wgettext!("Error reading script file '%s':"),
                    path.to_string_lossy()
                );
                eprintf!("%s\n", e);
            }
            Ok(f) => {
                let list = &args[my_optind..];
                parser.vars().set(
                    L!("argv"),
                    EnvMode::default(),
                    list.iter().map(|s| s.to_owned()).collect(),
                );
                let rel_filename = &args[my_optind - 1];
                let _filename_push = parser
                    .library_data
                    .scoped_set(Some(Arc::new(rel_filename.to_owned())), |s| {
                        &mut s.current_filename
                    });
                res = reader_read(parser, f.as_raw_fd(), &IoChain::new());
                if res.is_err() {
                    FLOGF!(
                        warning,
                        wgettext!("Error while reading file %s\n"),
                        path.to_string_lossy()
                    );
                }
            }
        }
    }

    let exit_status = if res.is_err() {
        STATUS_CMD_UNKNOWN
    } else {
        parser.get_last_status()
    };

    event::fire(parser, Event::process_exit(Pid::new(getpid()), exit_status));

    // Trigger any exit handlers.
    event::fire_generic(
        parser,
        L!("fish_exit").to_owned(),
        vec![exit_status.to_wstring()],
    );

    if let Some(profile_output) = opts.profile_output {
        parser.emit_profiling(&profile_output);
    }

    history::save_all();
    if opts.print_rusage_self {
        print_rusage_self();
    }

    exit_status
}

// https://github.com/fish-shell/fish-shell/issues/367
fn escape_single_quoted_hack_hack_hack_hack(s: &wstr) -> OsString {
    let mut result = OsString::with_capacity(s.len() + 2);
    result.push("\'");
    for c in s.chars() {
        // Escape backslashes and single quotes only.
        if matches!(c, '\\' | '\'') {
            result.push("\\");
        }
        result.push(c.to_string())
    }
    result.push("\'");
    result
}

fn fish_xdm_login_hack_hack_hack_hack(cmds: &mut [OsString], args: &[WString]) -> bool {
    if cmds.len() != 1 {
        return false;
    }

    let cmd = &cmds[0];
    if cmd == "exec \"${@}\"" || cmd == "exec \"$@\"" {
        // We're going to construct a new command that starts with exec, and then has the
        // remaining arguments escaped.
        let mut new_cmd = OsString::from("exec");
        for arg in &args[1..] {
            new_cmd.push(" ");
            new_cmd.push(escape_single_quoted_hack_hack_hack_hack(arg));
        }

        cmds[0] = new_cmd;
        true
    } else {
        false
    }
}
