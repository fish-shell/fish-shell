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

// Delete this once we require Rust 1.74.
#![allow(unstable_name_collisions)]
#![allow(clippy::uninlined_format_args)]

#[cfg(feature = "installable")]
use fish::common::wcs2osstring;
#[allow(unused_imports)]
use fish::future::IsSomeAnd;
use fish::{
    ast::Ast,
    builtins::{
        fish_indent, fish_key_reader,
        shared::{
            BUILTIN_ERR_MISSING, BUILTIN_ERR_UNKNOWN, STATUS_CMD_ERROR, STATUS_CMD_OK,
            STATUS_CMD_UNKNOWN,
        },
    },
    common::{
        escape, get_executable_path, save_term_foreground_process_group, str2wcstring, wcs2string,
        PACKAGE_NAME, PROFILING_ACTIVE, PROGRAM_NAME,
    },
    env::{
        environment::{env_init, EnvStack, Environment},
        ConfigPaths, EnvMode, Statuses,
    },
    eprintf,
    event::{self, Event},
    flog::{self, activate_flog_categories_by_pattern, set_flog_file_fd, FLOG, FLOGF},
    fprintf, function, future_feature_flags as features,
    history::{self, start_private_mode},
    io::IoChain,
    nix::{getpid, getrusage, isatty, RUsage},
    panic::panic_handler,
    parse_constants::{ParseErrorList, ParseTreeFlags},
    parse_tree::ParsedSource,
    parse_util::parse_util_detect_errors_in_ast,
    parser::{BlockType, CancelBehavior, Parser},
    path::path_get_config,
    printf,
    proc::{
        get_login, is_interactive_session, mark_login, mark_no_exec, proc_init,
        set_interactive_session, Pid,
    },
    reader::{reader_init, reader_read, term_copy_modes},
    signal::{signal_clear_cancel, signal_unblock_all},
    threads::{self},
    topic_monitor,
    wchar::prelude::*,
    wutil::waccess,
};
use std::ffi::{CString, OsStr, OsString};
use std::fs::File;
use std::os::unix::prelude::*;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::{env, ops::ControlFlow};

const DOC_DIR: &str = env!("DOCDIR");
const DATA_DIR: &str = env!("DATADIR");
const DATA_DIR_SUBDIR: &str = env!("DATADIR_SUBDIR");
const SYSCONF_DIR: &str = env!("SYSCONFDIR");
const BIN_DIR: &str = env!("BINDIR");

#[cfg(feature = "installable")]
// Disable for clippy because otherwise it would require sphinx
#[cfg(not(clippy))]
fn install(confirm: bool, dir: PathBuf) -> bool {
    use rust_embed::RustEmbed;

    #[derive(RustEmbed)]
    #[folder = "share/"]
    struct Asset;

    #[derive(RustEmbed)]
    #[folder = "target/man/man1"]
    #[prefix = "man/man1/"]
    struct Docs;

    use std::fs;
    use std::io::ErrorKind;
    use std::io::Write;
    use std::io::{stderr, stdin};

    // TODO: Translation,
    // FLOG?
    // - Install: Translations
    // - Install: Manpages (build via build.rs)
    // - Don't install: __fish_build_paths.fish.in
    if confirm {
        if isatty(libc::STDIN_FILENO) {
            eprintln!(
                "This will write fish's data files to '{}'.\n\
                 Please enter 'yes' to continue.",
                dir.display()
            );
            eprint!("> ");
            let _ = stderr().flush();
        }

        let mut input = String::new();
        if let Err(error) = stdin().read_line(&mut input) {
            eprintln!("error: {error}")
        }

        if input != "yes\n" {
            eprintln!("Exiting without writing any files\n");
            return false;
        }
    } else {
        eprintln!("Installing fish's data files to '{}'.", dir.display());
    }

    // Remove the install directory first, to clean out any removed files.
    if let Err(err) = fs::remove_dir_all(dir.clone()) {
        if err.kind() != ErrorKind::NotFound {
            eprintln!("Removing '{}' failed: {}", dir.display(), err);
            return false;
        }
    }

    // This function can't be top-level because rust_embed is an optional dependency, so it can't
    // be a part of the function signature.
    fn extract_embed<T: rust_embed::Embed>(dir: &Path) -> bool {
        for file in T::iter() {
            let path = dir.join(file.as_ref());
            let Ok(_) = fs::create_dir_all(path.parent().unwrap()) else {
                eprintln!(
                    "Creating directory '{}' failed",
                    path.parent().unwrap().display()
                );
                return false;
            };
            let res = File::create(&path);
            let Ok(mut f) = res else {
                eprintln!("Creating file '{}' failed", path.display());
                continue;
            };
            // This should be impossible.
            let d = T::get(&file).expect("File was somehow not included???");
            if let Err(error) = f.write_all(&d.data) {
                eprintln!("error: {error}");
                return false;
            }
        }
        return true;
    }

    if !extract_embed::<Asset>(&dir) {
        return false;
    }
    if !extract_embed::<Docs>(&dir) {
        return false;
    }

    let verfile = dir.join("fish-install-version");
    let res = File::create(&verfile);
    if let Ok(mut f) = res {
        f.write_all(fish::BUILD_VERSION.as_bytes())
            .expect("FAILED TO WRITE");
    } else {
        eprintln!("Creating file '{}' failed", verfile.display());
    };
    return true;
}

#[cfg(clippy)]
fn install(_confirm: bool, _dir: PathBuf) -> bool {
    unreachable!()
}

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
        if exec_path.starts_with(env!("CARGO_MANIFEST_DIR")) {
            let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
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
                bin: Some(exec_path.parent().unwrap().to_owned()),
            }
        }

        if !done {
            // The next check is that we are in a relocatable directory tree
            if exec_path.ends_with("bin/fish") {
                let base_path = exec_path.parent().unwrap().parent().unwrap();
                paths = ConfigPaths {
                    // One obvious path is ~/.local (with fish in ~/.local/bin/).
                    // If we picked ~/.local/share/fish as our data path,
                    // we would install there and erase history.
                    // So let's isolate us a bit more.
                    #[cfg(feature = "installable")]
                    data: base_path.join("share/fish/install"),
                    #[cfg(not(feature = "installable"))]
                    data: base_path.join("share/fish"),
                    sysconf: base_path.join("etc/fish"),
                    doc: base_path.join("share/doc/fish"),
                    bin: Some(base_path.join("bin")),
                }
            } else if exec_path.ends_with("fish") {
                FLOG!(
                    config,
                    "'fish' not in a 'bin/', trying paths relative to source tree"
                );
                let base_path = exec_path.parent().unwrap();
                paths = ConfigPaths {
                    #[cfg(feature = "installable")]
                    data: base_path.join("share/install"),
                    #[cfg(not(feature = "installable"))]
                    data: base_path.join("share"),
                    sysconf: base_path.join("etc"),
                    doc: base_path.join("user_doc/html"),
                    bin: Some(base_path.to_path_buf()),
                }
            }

            if paths.data.exists() && paths.sysconf.exists() {
                // The docs dir may not exist; in that case fall back to the compiled in path.
                if !paths.doc.exists() {
                    paths.doc = PathBuf::from(DOC_DIR);
                }
                done = true;
            }
        }
    }

    if !done {
        // Fall back to what got compiled in.
        let data = if cfg!(feature = "installable") {
            let Some(home) = fish::env::get_home() else {
                FLOG!(
                    error,
                    "Cannot find home directory and will refuse to read configuration.\n",
                    "Consider installing into a directory tree with `fish --install=PATH`."
                );
                return paths;
            };

            PathBuf::from(home).join(DATA_DIR).join(DATA_DIR_SUBDIR)
        } else {
            Path::new(DATA_DIR).join(DATA_DIR_SUBDIR)
        };
        let bin = if cfg!(feature = "installable") {
            exec_path.parent().map(|x| x.to_path_buf())
        } else {
            Some(PathBuf::from(BIN_DIR))
        };

        FLOG!(config, "Using compiled in paths:");
        paths = ConfigPaths {
            data,
            sysconf: Path::new(SYSCONF_DIR).join("fish"),
            doc: DOC_DIR.into(),
            bin,
        }
    }

    FLOGF!(
        config,
        "determine_config_directory_paths() results:\npaths.data: %ls\npaths.sysconf: \
        %ls\npaths.doc: %ls\npaths.bin: %ls",
        paths.data.display().to_string(),
        paths.sysconf.display().to_string(),
        paths.doc.display().to_string(),
        paths
            .bin
            .clone()
            .map(|x| x.display().to_string())
            .unwrap_or("|not found|".to_string()),
    );

    paths
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
            "not sourcing %ls (not readable or does not exist)",
            escaped_pathname
        );
        return false;
    }
    FLOG!(config, "sourcing", escaped_pathname);

    let cmd: WString = L!("builtin source ").to_owned() + escaped_pathname.as_utfstr();

    parser.libdata_mut().within_fish_init = true;
    let _ = parser.eval(&cmd, &IoChain::new());
    parser.libdata_mut().within_fish_init = false;
    return true;
}

#[cfg(feature = "installable")]
fn check_version_file(paths: &ConfigPaths, datapath: &wstr) -> Option<bool> {
    // (false-positive, is_none_or is a backport, this builds with 1.70)
    #[allow(clippy::incompatible_msrv)]
    if paths
        .bin
        .clone()
        .is_none_or(|x| !x.starts_with(env!("CARGO_MANIFEST_DIR")))
    {
        // When fish is installable, we write the version to a file,
        // now we check it.
        let verfile = PathBuf::from(wcs2osstring(datapath)).join("fish-install-version");
        let version = std::fs::read_to_string(verfile).ok()?;

        return Some(version == fish::BUILD_VERSION);
    }
    // When running from the manifest dir, we'll just run.
    return Some(true);
}

/// Parse init files. exec_path is the path of fish executable as determined by argv[0].
fn read_init(parser: &Parser, paths: &ConfigPaths) {
    let datapath = str2wcstring(paths.data.as_os_str().as_bytes());

    #[cfg(feature = "installable")]
    {
        // If the version file is non-existent or out of date,
        // we try to install automatically, but only if we're interactive.
        // If we're not interactive, we still print an error later on pointing to `--install` if they don't exist,
        // but don't complain if they're merely out-of-date.
        // We do specifically check for a tty because we want to read input to confirm.
        let v = check_version_file(paths, &datapath);

        #[allow(clippy::incompatible_msrv)]
        if v.is_none_or(|x| !x) && is_interactive_session() && isatty(libc::STDIN_FILENO) {
            if v.is_none() {
                FLOG!(
                    warning,
                    "Fish's asset files are missing. Trying to install them."
                );
            } else {
                FLOG!(
                    warning,
                    "Fish's asset files are out of date. Trying to install them."
                );
            }

            install(true, PathBuf::from(wcs2osstring(&datapath)));
            // We try to go on if installation failed (or was rejected) here
            // If the assets are missing, we will trigger a later error,
            // if they are outdated, things will probably (tm) work somewhat.
        }
    }
    if !source_config_in_directory(parser, &datapath) {
        // If we cannot read share/config.fish, our internal configuration,
        // something is wrong.
        // That also means that our functions won't be found,
        // and so any config we get would almost certainly be broken.
        let escaped_pathname = escape(&datapath);
        FLOGF!(
            error,
            "Fish cannot find its asset files in '%ls'.\n\
             Refusing to read configuration because of this.",
            escaped_pathname,
        );
        #[cfg(feature = "installable")]
        FLOG!(
            error,
            "If you installed via `cargo install`, please run `fish --install` and restart fish."
        );
        return;
    }
    source_config_in_directory(parser, &str2wcstring(paths.sysconf.as_os_str().as_bytes()));

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
    use fish::wgetopt::{wopt, ArgType::*, WGetopter, WOption};

    const RUSAGE_ARG: char = 1 as char;
    const PRINT_DEBUG_CATEGORIES_ARG: char = 2 as char;
    const PROFILE_STARTUP_ARG: char = 3 as char;

    const SHORT_OPTS: &wstr = L!("+:hPilNnvc:C:p:d:f:D:o:");
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
        wopt(L!("install"), OptionalArgument, 'I'),
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
                .push(OsString::from_vec(wcs2string(w.woptarg.unwrap()))),
            'C' => opts
                .postconfig_cmds
                .push(OsString::from_vec(wcs2string(w.woptarg.unwrap()))),
            'd' => {
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
            'I' => {
                #[cfg(not(feature = "installable"))]
                eprintln!("Fish was built without support for self-installation");
                #[cfg(feature = "installable")]
                if let Some(path) = w.woptarg {
                    // We were given an explicit path.
                    // Install us there as a relocatable install.
                    // That means:
                    // path/bin/fish is the fish binary
                    // path/share/fish/ is the data directory
                    // path/etc/fish is sysconf????
                    use std::fs;
                    let dir = PathBuf::from(wcs2osstring(path));
                    if install(true, dir.join("share/fish/install")) {
                        for sub in &["share/fish/install", "etc/fish", "bin"] {
                            let p = dir.join(sub);
                            let Ok(_) = fs::create_dir_all(p.clone()) else {
                                eprintln!("Creating directory '{}' failed", p.display());
                                std::process::exit(1);
                            };
                        }

                        // Copy ourselves there.
                        let argv0 = OsString::from_vec(wcs2string(&args[0]));
                        let exec_path =
                            get_executable_path(<OsString as AsRef<Path>>::as_ref(&argv0));
                        let binpath = dir.join("bin/fish");
                        if let Ok(exec_path) = exec_path.canonicalize() {
                            if exec_path != binpath {
                                if let Err(err) = std::fs::copy(exec_path, binpath.clone()) {
                                    FLOG!(error, "Cannot copy fish to", binpath.display());
                                    FLOG!(error, err);
                                    std::process::exit(1);
                                }
                                println!(
                                    "Fish installed in '{}'. Start that from now on.",
                                    binpath.display()
                                );
                                // TODO: Reexec fish?
                                std::process::exit(0);
                            }
                        } else {
                            FLOG!(error, "Cannot copy fish to '%ls'. Please copy the fish binary there manually", binpath.display());
                        }
                    }
                } else {
                    let paths = Some(determine_config_directory_paths(OsString::from_vec(
                        wcs2string(&args[0]),
                    )));
                    let Some(paths) = paths else {
                        FLOG!(error, "Cannot find config paths");
                        std::process::exit(1);
                    };
                    install(true, paths.data);
                }
            }
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
                return ControlFlow::Break(0);
            }
            // "--profile" - this does not activate profiling right away,
            // rather it's done after startup is finished.
            'p' => opts.profile_output = Some(OsString::from_vec(wcs2string(w.woptarg.unwrap()))),
            PROFILE_STARTUP_ARG => {
                // With "--profile-startup" we immediately turn profiling on.
                opts.profile_startup_output =
                    Some(OsString::from_vec(wcs2string(w.woptarg.unwrap())));
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
                eprintln!(
                    "{}",
                    wgettext_fmt!(BUILTIN_ERR_UNKNOWN, "fish", args[w.wopt_index - 1])
                );
                return ControlFlow::Break(1);
            }
            ':' => {
                eprintln!(
                    "{}",
                    wgettext_fmt!(BUILTIN_ERR_MISSING, "fish", args[w.wopt_index - 1])
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
    if opts.batch_cmds.is_empty() && optind == args.len() && isatty(libc::STDIN_FILENO) {
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

    {
        let s = CString::new("").unwrap();
        unsafe {
            libc::setlocale(libc::LC_ALL, s.as_ptr());
        }
    }

    let mut args: Vec<WString> = env::args_os()
        .map(|osstr| str2wcstring(osstr.as_bytes()))
        .collect();
    if args.is_empty() {
        args.push("fish".into());
    }

    // Enable debug categories set in FISH_DEBUG.
    // This is in *addition* to the ones given via --debug.
    if let Some(debug_categories) = env::var_os("FISH_DEBUG") {
        let s = str2wcstring(debug_categories.as_bytes());
        activate_flog_categories_by_pattern(&s);
    }

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
                // TODO: should not be debug-print
                eprintln!("Could not open file {:?}", debug_path);
                eprintln!("{}", e);
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
    let paths: Option<ConfigPaths> = if !opts.no_exec {
        let paths = Some(determine_config_directory_paths(OsString::from_vec(
            wcs2string(&args[0]),
        )));
        env_init(
            paths.as_ref(),
            /* do uvars */ !opts.no_config,
            /* default paths */ opts.no_config,
        );
        paths
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
    proc_init();
    fish::env::misc_init();
    reader_init(true);

    // Construct the root parser!
    let env = Rc::new(EnvStack::globals().create_child(true /* dispatches_var_changes */));
    let parser = &Parser::new(env, CancelBehavior::Clear);
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
            let _ = run_command_list(parser, &[OsString::from("fish_default_key_bindings")]);
        }
    }

    // Re-read the terminal modes after config, it might have changed them.
    term_copy_modes();

    // Stomp the exit status of any initialization commands (issue #635).
    parser.set_last_statuses(Statuses::just(STATUS_CMD_OK));

    // TODO: if-let-chains
    if opts.profile_startup_output.is_some() && opts.profile_startup_output != opts.profile_output {
        parser.emit_profiling(&opts.profile_startup_output.unwrap());

        // If we are profiling both, ensure the startup data only
        // ends up in the startup file.
        parser.clear_profiling();
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
                        wgettext!("Error while reading file %ls\n"),
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

    event::fire(
        parser,
        Event::process_exit(Pid::new(getpid()).unwrap(), exit_status),
    );

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
