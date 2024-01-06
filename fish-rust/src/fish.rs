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

use crate::{
    ast::Ast,
    builtins::shared::{
        BUILTIN_ERR_MISSING, BUILTIN_ERR_UNKNOWN, STATUS_CMD_OK, STATUS_CMD_UNKNOWN,
    },
    common::{
        escape, exit_without_destructors, get_executable_path,
        restore_term_foreground_process_group_for_exit, save_term_foreground_process_group,
        scoped_push_replacer, str2wcstring, wcs2string, PROFILING_ACTIVE, PROGRAM_NAME,
    },
    env::Statuses,
    env::{
        environment::{env_init, EnvStack, Environment},
        ConfigPaths, EnvMode,
    },
    event::{self, Event},
    fds::set_cloexec,
    ffi::{self},
    flog::{self, activate_flog_categories_by_pattern, set_flog_file_fd, FLOG, FLOGF},
    function, future_feature_flags as features, history,
    history::start_private_mode,
    io::IoChain,
    nix::{getpid, isatty},
    parse_constants::{ParseErrorList, ParseTreeFlags},
    parse_tree::ParsedSource,
    parse_util::parse_util_detect_errors_in_ast,
    parser::{BlockType, Parser},
    path::path_get_config,
    proc::{
        get_login, is_interactive_session, mark_login, mark_no_exec, proc_init,
        set_interactive_session,
    },
    reader::{reader_init, reader_read, restore_term_mode, term_copy_modes},
    signal::{signal_clear_cancel, signal_unblock_all},
    threads::{self, asan_maybe_exit},
    topic_monitor,
    wchar::prelude::*,
    wutil::waccess,
};
use std::env;
use std::ffi::{CString, OsStr, OsString};
use std::fs::File;
use std::mem::MaybeUninit;
use std::os::unix::prelude::*;
use std::path::{Path, PathBuf};
use std::sync::atomic::Ordering;
use std::sync::Arc;

// FIXME: when the crate is actually called fish and not fish-rust, read this from cargo
// See: https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-crates
// for reference
const PACKAGE_NAME: &str = "fish"; // env!("CARGO_PKG_NAME");

// FIXME: the following should just use env!(), this is to make `cargo test` work without CMake for now
const DOC_DIR: &str = {
    match option_env!("DOCDIR") {
        Some(e) => e,
        None => "(unused)",
    }
};
const DATA_DIR: &str = {
    match option_env!("DATADIR") {
        Some(e) => e,
        None => "(unused)",
    }
};
const SYSCONF_DIR: &str = {
    match option_env!("SYSCONFDIR") {
        Some(e) => e,
        None => "(unused)",
    }
};
const BIN_DIR: &str = {
    match option_env!("BINDIR") {
        Some(e) => e,
        None => "(unused)",
    }
};

// C++ had this as optional, and used CMAKE_BINARY_DIR,
// should probably be swapped to `OUT_DIR` once CMake is gone?
// const OUT_DIR: &str = env!("OUT_DIR", "OUT_DIR was not specified");
const OUT_DIR: &str = env!("FISH_BUILD_DIR");

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

/// \return a timeval converted to milliseconds.
#[allow(clippy::unnecessary_cast)]
fn tv_to_msec(tv: &libc::timeval) -> i64 {
    // milliseconds per second
    let mut msec = tv.tv_sec as i64 * 1000;
    // microseconds per millisecond
    msec += tv.tv_usec as i64 / 1000;
    msec
}

fn print_rusage_self() {
    let mut rs = MaybeUninit::uninit();
    if unsafe { libc::getrusage(libc::RUSAGE_SELF, rs.as_mut_ptr()) } != 0 {
        let s = CString::new("getrusage").unwrap();
        unsafe { libc::perror(s.as_ptr()) }
        return;
    }
    let rs: libc::rusage = unsafe { rs.assume_init() };
    let rss_kb = if cfg!(target_os = "macos") {
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

    eprintln!("  rusage self:");
    eprintln!("      user time: {sys_time} ms");
    eprintln!("       sys time: {user_time} ms");
    eprintln!("     total time: {total_time} ms");
    eprintln!("        max rss: {rss_kb} kb");
    eprintln!("        signals: {signals}");
}

fn determine_config_directory_paths(argv0: impl AsRef<Path>) -> ConfigPaths {
    // PORTING: why is this not just an associated method on ConfigPaths?

    let mut paths = ConfigPaths::default();
    let mut done = false;
    let exec_path = get_executable_path(argv0.as_ref());
    if let Ok(exec_path) = exec_path.canonicalize() {
        FLOG!(
            config,
            format!("exec_path: {:?}, argv[0]: {:?}", exec_path, argv0.as_ref())
        );
        // TODO: we should determine program_name from argv0 somewhere in this file

        // Detect if we're running right out of the CMAKE build directory
        if exec_path.starts_with(OUT_DIR) {
            let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
            FLOG!(
                config,
                "Running out of target directory, using paths relative to CARGO_MANIFEST_DIR:\n",
                manifest_dir.display()
            );
            done = true;
            paths = ConfigPaths {
                data: manifest_dir.join("share"),
                sysconf: manifest_dir.join("etc"),
                doc: manifest_dir.join("user_doc/html"),
                bin: OUT_DIR.into(),
            }
        }

        if !done {
            // The next check is that we are in a reloctable directory tree
            let installed_suffix = Path::new("/bin/fish");
            let just_a_fish = Path::new("/fish");
            let suffix = if exec_path.ends_with(installed_suffix) {
                Some(installed_suffix)
            } else if exec_path.ends_with(just_a_fish) {
                FLOG!(
                    config,
                    "'fish' not in a 'bin/', trying paths relative to source tree"
                );
                Some(just_a_fish)
            } else {
                None
            };

            if let Some(suffix) = suffix {
                let seems_installed = suffix == installed_suffix;

                let mut base_path = exec_path;
                base_path.shrink_to(base_path.as_os_str().len() - suffix.as_os_str().len());
                let base_path = base_path;

                paths = if seems_installed {
                    ConfigPaths {
                        data: base_path.join("share/fish"),
                        sysconf: base_path.join("etc/fish"),
                        doc: base_path.join("share/doc/fish"),
                        bin: base_path.join("bin"),
                    }
                } else {
                    ConfigPaths {
                        data: base_path.join("share"),
                        sysconf: base_path.join("etc"),
                        doc: base_path.join("user_doc/html"),
                        bin: base_path,
                    }
                };

                if paths.data.exists() && paths.sysconf.exists() {
                    // The docs dir may not exist; in that case fall back to the compiled in path.
                    if !paths.doc.exists() {
                        paths.doc = PathBuf::from(DOC_DIR);
                    }
                    done = true;
                }
            }
        }
    }

    if !done {
        // Fall back to what got compiled in.
        FLOG!(config, "Using compiled in paths:");
        paths = ConfigPaths {
            data: PathBuf::from(DATA_DIR).join("fish"),
            sysconf: PathBuf::from(SYSCONF_DIR).join("fish"),
            doc: DOC_DIR.into(),
            bin: BIN_DIR.into(),
        }
    }

    FLOGF!(
        config,
        "determine_config_directory_paths() results:\npaths.data: %ls\npaths.sysconf: \
        %ls\npaths.doc: %ls\npaths.bin: %ls",
        paths.data.display().to_string(),
        paths.sysconf.display().to_string(),
        paths.doc.display().to_string(),
        paths.bin.display().to_string()
    );

    paths
}

// Source the file config.fish in the given directory.
fn source_config_in_directory(parser: &Parser, dir: &wstr) {
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
            "not sourcing %ls (not readable or does not exist)",
            escaped_pathname
        );
        return;
    }
    FLOG!(config, "sourcing", escaped_pathname);

    let cmd: WString = L!("builtin source ").to_owned() + escaped_pathname.as_utfstr();

    parser.libdata_mut().pods.within_fish_init = true;
    let _ = parser.eval(&cmd, &IoChain::new());
    parser.libdata_mut().pods.within_fish_init = false;
}

/// Parse init files. exec_path is the path of fish executable as determined by argv[0].
fn read_init(parser: &Parser, paths: &ConfigPaths) {
    source_config_in_directory(parser, &str2wcstring(paths.data.as_os_str().as_bytes()));
    source_config_in_directory(parser, &str2wcstring(paths.sysconf.as_os_str().as_bytes()));

    // We need to get the configuration directory before we can source the user configuration file.
    // If path_get_config returns false then we have no configuration directory and no custom config
    // to load.
    if let Some(config_dir) = path_get_config() {
        source_config_in_directory(parser, &config_dir);
    }
}

fn run_command_list(parser: &Parser, cmds: &[OsString]) -> i32 {
    let mut retval = STATUS_CMD_OK;
    for cmd in cmds {
        let cmd_wcs = str2wcstring(cmd.as_bytes());

        let mut errors = ParseErrorList::new();
        let ast = Ast::parse(&cmd_wcs, ParseTreeFlags::empty(), Some(&mut errors));
        let errored = ast.errored() || {
            parse_util_detect_errors_in_ast(&ast, &cmd_wcs, Some(&mut errors)).is_err()
        };

        if !errored {
            // Construct a parsed source ref.
            let ps = Arc::new(ParsedSource::new(cmd_wcs, ast));
            let _ = parser.eval_parsed_source(&ps, &IoChain::new(), None, BlockType::top);
            retval = STATUS_CMD_OK;
        } else {
            let backtrace = parser.get_backtrace(&cmd_wcs, &errors);
            eprintf!("%s", backtrace);
            // XXX: Why is this the return for "unknown command"?
            retval = STATUS_CMD_UNKNOWN;
        }
    }

    retval.unwrap()
}

fn fish_parse_opt(args: &mut [WString], opts: &mut FishCmdOpts) -> usize {
    use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t::*};

    const RUSAGE_ARG: char = 1 as char;
    const PRINT_DEBUG_CATEGORIES_ARG: char = 2 as char;
    const PROFILE_STARTUP_ARG: char = 3 as char;

    const SHORT_OPTS: &wstr = L!("+hPilNnvc:C:p:d:f:D:o:");
    const LONG_OPTS: &[woption<'static>] = &[
        wopt(L!("command"), required_argument, 'c'),
        wopt(L!("init-command"), required_argument, 'C'),
        wopt(L!("features"), required_argument, 'f'),
        wopt(L!("debug"), required_argument, 'd'),
        wopt(L!("debug-output"), required_argument, 'o'),
        wopt(L!("debug-stack-frames"), required_argument, 'D'),
        wopt(L!("interactive"), no_argument, 'i'),
        wopt(L!("login"), no_argument, 'l'),
        wopt(L!("no-config"), no_argument, 'N'),
        wopt(L!("no-execute"), no_argument, 'n'),
        wopt(L!("print-rusage-self"), no_argument, RUSAGE_ARG),
        wopt(
            L!("print-debug-categories"),
            no_argument,
            PRINT_DEBUG_CATEGORIES_ARG,
        ),
        wopt(L!("profile"), required_argument, 'p'),
        wopt(
            L!("profile-startup"),
            required_argument,
            PROFILE_STARTUP_ARG,
        ),
        wopt(L!("private"), no_argument, 'P'),
        wopt(L!("help"), no_argument, 'h'),
        wopt(L!("version"), no_argument, 'v'),
    ];

    let mut shim_args: Vec<&wstr> = args.iter().map(|s| s.as_ref()).collect();
    let mut w = wgetopter_t::new(SHORT_OPTS, LONG_OPTS, &mut shim_args);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'c' => opts
                .batch_cmds
                .push(OsString::from_vec(wcs2string(w.woptarg.unwrap()))),
            'C' => opts
                .postconfig_cmds
                .push(OsString::from_vec(wcs2string(w.woptarg.unwrap()))),
            'd' => {
                ffi::activate_flog_categories_by_pattern(w.woptarg.unwrap());
                activate_flog_categories_by_pattern(w.woptarg.unwrap());
                for cat in flog::categories::all_categories() {
                    if cat.enabled.load(Ordering::Relaxed) {
                        println!("Debug enabled for category: {}", cat.name);
                    }
                }
            }
            'o' => opts.debug_output = Some(OsString::from_vec(wcs2string(w.woptarg.unwrap()))),
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
                    let desc = wgettext_str(cat.description);
                    // https://doc.rust-lang.org/std/fmt/#syntax
                    // this is left-justified
                    println!("{:<width$} {}", cat.name, desc, width = name_width);
                }
                std::process::exit(0);
            }
            // "--profile" - this does not activate profiling right away,
            // rather it's done after startup is finished.
            'p' => opts.profile_output = Some(OsString::from_vec(wcs2string(w.woptarg.unwrap()))),
            PROFILE_STARTUP_ARG => {
                // With "--profile-startup" we immediately turn profiling on.
                opts.profile_startup_output =
                    Some(OsString::from_vec(wcs2string(w.woptarg.unwrap())));
                PROFILING_ACTIVE.store(true);
                ffi::set_profiling_active(true);
            }
            'P' => opts.enable_private_mode = true,
            'v' => {
                printf!(
                    "%s",
                    wgettext_fmt!("%s, version %s\n", PACKAGE_NAME, crate::BUILD_VERSION)
                );
                std::process::exit(0);
            }
            'D' => {
                // TODO: Option is currently useless.
                // Either remove it or make it work with FLOG.
            }
            '?' => {
                eprintln!(
                    "{}",
                    wgettext_fmt!(BUILTIN_ERR_UNKNOWN, "fish", args[w.woptind - 1])
                );
                std::process::exit(1)
            }
            ':' => {
                eprintln!(
                    "{}",
                    wgettext_fmt!(BUILTIN_ERR_MISSING, "fish", args[w.woptind - 1])
                );
                std::process::exit(1)
            }
            _ => panic!("unexpected retval from wgetopter_t"),
        }
    }
    let optind = w.woptind;

    // If our command name begins with a dash that implies we're a login shell.
    opts.is_login |= args[0].char_at(0) == '-';

    // We are an interactive session if we have not been given an explicit
    // command or file to execute and stdin is a tty. Note that the -i or
    // --interactive options also force interactive mode.
    if opts.batch_cmds.is_empty() && optind == args.len() && isatty(libc::STDIN_FILENO) {
        set_interactive_session(true);
    }

    optind
}

fn cstr_from_osstr(s: &OsStr) -> CString {
    // is there no better way to do this?
    // this is
    // CStr::from_bytes_until_nul(s.as_bytes()).unwrap()
    // except we need to add the nul if it is not present
    CString::new(
        s.as_bytes()
            .iter()
            .cloned()
            .take_while(|&c| c != b'\0')
            .collect::<Vec<_>>(),
    )
    .unwrap()
}

fn main() -> i32 {
    let mut args: Vec<WString> = env::args_os()
        .map(|osstr| str2wcstring(osstr.as_bytes()))
        .collect();

    PROGRAM_NAME
        .set(L!("fish"))
        .expect("multiple entrypoints setting PROGRAM_NAME");

    let mut res = 1;
    let mut my_optind;

    signal_unblock_all();
    topic_monitor::topic_monitor_init();
    threads::init();

    {
        let s = CString::new("").unwrap();
        unsafe {
            libc::setlocale(libc::LC_ALL, s.as_ptr());
        }
    }

    if args.is_empty() {
        args.push("fish".into());
    }

    // Enable debug categories set in FISH_DEBUG.
    // This is in *addition* to the ones given via --debug.
    if let Some(debug_categories) = env::var_os("FISH_DEBUG") {
        let s = str2wcstring(debug_categories.as_bytes());
        activate_flog_categories_by_pattern(&s);
        ffi::activate_flog_categories_by_pattern(s);
    }

    let mut opts = FishCmdOpts::default();
    my_optind = fish_parse_opt(&mut args, &mut opts);

    // Direct any debug output right away.
    // --debug-output takes precedence, otherwise $FISH_DEBUG_OUTPUT is used.
    // PORTING: this is a slight difference from C++, we now skip reading the env var if the argument is an empty string
    if opts.debug_output.is_none() {
        opts.debug_output = env::var_os("FISH_DEBUG_OUTPUT");
    }

    let mut debug_output = std::ptr::null_mut();
    if let Some(debug_path) = opts.debug_output {
        let path = cstr_from_osstr(&debug_path);
        let mode = CString::new("w").unwrap();
        let debug_file = unsafe { libc::fopen(path.as_ptr(), mode.as_ptr()) };

        if debug_file.is_null() {
            eprintln!("Could not open file {:?}", debug_output);
            let s = CString::new("fopen").unwrap();
            unsafe { libc::perror(s.as_ptr()) }
            std::process::exit(-1);
        }

        set_cloexec(unsafe { libc::fileno(debug_file) }, true);
        ffi::flog_setlinebuf_ffi(debug_file as *mut _);
        ffi::set_flog_output_file_ffi(debug_file as *mut _);
        set_flog_file_fd(unsafe { libc::fileno(debug_file) });

        debug_output = debug_file;

        /* TODO: just use File when C++ does not need a *mut FILE

        debug_output = match File::options()
            .write(true)
            .truncate(true)
            .create(true)
            .open(debug_path)
        {
            Ok(dbg_file) => {
                // Rust sets O_CLOEXEC by default
                // https://github.com/rust-lang/rust/blob/07438b0928c6691d6ee734a5a77823ec143be94d/library/std/src/sys/unix/fs.rs#L1059

                flog::set_flog_file_fd(dbg_file.as_raw_fd());
                Some(dbg_file)
            }
            Err(e) => {
                // TODO: should not be debug-print
                eprintln!("Could not open file {:?}", debug_output);
                eprintln!("{}", e);
                std::process::exit(1);
            }
        };
        */
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

    let mut paths: Option<ConfigPaths> = None;
    // If we're not executing, there's no need to find the config.
    if !opts.no_exec {
        // PORTING: C++ had not converted, we must revert
        paths = Some(determine_config_directory_paths(OsString::from_vec(
            wcs2string(&args[0]),
        )));
        env_init(
            paths.as_ref(),
            /* do uvars */ !opts.no_config,
            /* default paths */ opts.no_config,
        );
    }

    // Set features early in case other initialization depends on them.
    // Start with the ones set in the environment, then those set on the command line (so the
    // command line takes precedence).
    if let Some(features_var) = EnvStack::globals().get(L!("fish_features")) {
        for s in features_var.as_list() {
            features::set_from_string(s.as_utfstr());
        }
    }
    features::set_from_string(opts.features.as_utfstr());
    proc_init();
    crate::env::misc_init();
    reader_init();

    let parser = Parser::principal_parser();
    parser.set_syncs_uvars(!opts.no_config);

    if !opts.no_exec && !opts.no_config {
        read_init(parser, paths.as_ref().unwrap());
    }

    if is_interactive_session() && opts.no_config && !opts.no_exec {
        // If we have no config, we default to the default key bindings.
        parser.vars().set_one(
            L!("fish_key_bindings"),
            EnvMode::UNEXPORT,
            L!("fish_default_key_bindings").to_owned(),
        );
        if function::exists(L!("fish_default_key_bindings"), parser) {
            run_command_list(parser, &[OsString::from("fish_default_key_bindings")]);
        }
    }

    // Re-read the terminal modes after config, it might have changed them.
    term_copy_modes();

    // Stomp the exit status of any initialization commands (issue #635).
    parser.set_last_statuses(Statuses::just(STATUS_CMD_OK.unwrap()));

    // TODO: if-let-chains
    if opts.profile_startup_output.is_some() && opts.profile_startup_output != opts.profile_output {
        let s = cstr_from_osstr(&opts.profile_startup_output.unwrap());
        parser.emit_profiling(s.as_bytes());

        // If we are profiling both, ensure the startup data only
        // ends up in the startup file.
        parser.clear_profiling();
    }

    PROFILING_ACTIVE.store(opts.profile_output.is_some());
    ffi::set_profiling_active(opts.profile_output.is_some());

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
        parser.libdata_mut().pods.exit_current_script = false;
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
        // C++ had not converted at this point, we must undo
        let n = wcs2string(&args[my_optind]);
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
                eprintln!("{}", e);
            }
            Ok(f) => {
                // PORTING: the args were converted to WString here in C++
                let list = &args[my_optind..];
                parser.vars().set(
                    L!("argv"),
                    EnvMode::default(),
                    list.iter().map(|s| s.to_owned()).collect(),
                );
                let rel_filename = &args[my_optind - 1];
                let _filename_push = scoped_push_replacer(
                    |new_value| {
                        std::mem::replace(&mut parser.libdata_mut().current_filename, new_value)
                    },
                    Some(Arc::new(rel_filename.to_owned())),
                );
                res = reader_read(parser, f.as_raw_fd(), &IoChain::new());
                if res != 0 {
                    FLOGF!(
                        warning,
                        wgettext!("Error while reading file %ls\n"),
                        path.to_string_lossy()
                    );
                }
            }
        }
    }

    let exit_status = if res != 0 {
        STATUS_CMD_UNKNOWN.unwrap()
    } else {
        parser.get_last_status()
    };

    event::fire(parser, Event::process_exit(getpid(), exit_status));

    // Trigger any exit handlers.
    event::fire_generic(
        parser,
        L!("fire_exit").to_owned(),
        vec![exit_status.to_wstring()],
    );

    restore_term_mode();
    // this is ported, but not adopted
    restore_term_foreground_process_group_for_exit();

    if let Some(profile_output) = opts.profile_output {
        let s = cstr_from_osstr(&profile_output);
        parser.emit_profiling(s.as_bytes());
    }

    history::save_all();
    if opts.print_rusage_self {
        print_rusage_self();
    }

    if !debug_output.is_null() {
        unsafe { libc::fclose(debug_output) };
    }

    asan_maybe_exit(exit_status);
    exit_without_destructors(exit_status)
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
    return result;
}

fn fish_xdm_login_hack_hack_hack_hack(cmds: &mut Vec<OsString>, args: &[WString]) -> bool {
    if cmds.len() != 1 {
        return false;
    }

    let mut result = false;
    let cmd = cmds.get(0).unwrap();
    if cmd == "exec \"${@}\"" || cmd == "exec \"$@\"" {
        // We're going to construct a new command that starts with exec, and then has the
        // remaining arguments escaped.
        let mut new_cmd = OsString::from("exec");
        for arg in &args[1..] {
            new_cmd.push(" ");
            new_cmd.push(escape_single_quoted_hack_hack_hack_hack(arg));
        }

        cmds[0] = new_cmd;
        result = true;
    }
    return result;
}

#[cxx::bridge]
mod fish_ffi {
    extern "Rust" {
        #[cxx_name = "rust_main"]
        fn main() -> i32;
    }
}
