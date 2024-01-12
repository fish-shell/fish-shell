use rsconf::{LinkType, Target};
use std::env;
use std::error::Error;
use std::path::{Path, PathBuf};
use std::process::Stdio;

fn main() {
    for key in ["DOCDIR", "DATADIR", "SYSCONFDIR", "BINDIR", "LOCALEDIR"] {
        if let Ok(val) = env::var(key) {
            // Forward some CMake config
            if val.is_empty() {
                panic!("{} is empty!", key);
            }
            println!("cargo:rustc-env={key}={val}");
        }
    }

    let rust_dir = env!("CARGO_MANIFEST_DIR");
    // Add our default to enable tools that don't go through CMake, like "cargo test" and the
    // language server.
    let build_dir =
        PathBuf::from(std::env::var("FISH_BUILD_DIR").unwrap_or(format!("{rust_dir}/build")));
    println!("cargo:rustc-env=FISH_BUILD_DIR={}", build_dir.display());

    let cached_curses_libnames = Path::join(&build_dir, "cached-curses-libnames");
    let curses_libnames: Vec<String> = if let Ok(lib_path_list) = env::var("CURSES_LIBRARY_LIST") {
        let lib_paths = lib_path_list
            .split(',')
            .filter(|s| !s.is_empty())
            .map(|s| s.to_string());
        let curses_libnames: Vec<_> = lib_paths
            .map(|libpath| {
                let stem = Path::new(&libpath).file_stem().unwrap().to_str().unwrap();
                // Ubuntu-32bit-fetched-pcre2's ncurses doesn't have the "lib" prefix.
                stem.strip_prefix("lib").unwrap_or(stem).to_owned()
            })
            .collect();
        std::fs::write(cached_curses_libnames, curses_libnames.join("\n") + "\n").unwrap();
        curses_libnames
    } else {
        let lib_cache = std::fs::read(cached_curses_libnames).unwrap_or_default();
        let lib_cache = String::from_utf8(lib_cache).unwrap();
        lib_cache
            .split('\n')
            .filter(|s| !s.is_empty())
            .map(|s| s.to_owned())
            .collect()
    };
    rsconf::link_libraries(&curses_libnames, LinkType::Default);

    cc::Build::new()
        .file("fish-rust/src/libc.c")
        .include(&build_dir)
        .compile("flibc.a");

    if compiles("fish-rust/src/cfg/w_exitcode.cpp") {
        println!("cargo:rustc-cfg=HAVE_WAITSTATUS_SIGNAL_RET");
    }
    if compiles("fish-rust/src/cfg/eventfd.c") {
        println!("cargo:rustc-cfg=HAVE_EVENTFD");
    }
    if compiles("fish-rust/src/cfg/pipe2.c") {
        println!("cargo:rustc-cfg=HAVE_PIPE2");
    }
    if compiles("fish-rust/src/cfg/spawn.c") {
        println!("cargo:rustc-cfg=FISH_USE_POSIX_SPAWN");
    }

    let mut build = cc::Build::new();
    // Add to the default library search path
    build.flag_if_supported("-L/usr/local/lib/");
    rsconf::add_library_search_path("/usr/local/lib");
    let mut detector = Target::new_from(build).unwrap();
    // Keep verbose mode on until we've ironed out rust build script stuff
    detector.set_verbose(true);
    detect_features(detector);
}

/// Dynamically enables certain features at build-time, without their having to be explicitly
/// enabled in the `cargo build --features xxx` invocation.
///
/// This can be used to enable features that we check for and conditionally compile according to in
/// our own codebase, but [can't be used to pull in dependencies](0) even if they're gated (in
/// `Cargo.toml`) behind a feature we just enabled.
///
/// [0]: https://github.com/rust-lang/cargo/issues/5499
fn detect_features(target: Target) {
    for (feature, handler) in [
        // Ignore the first entry, it just sets up the type inference. Model new entries after the
        // second line.
        (
            "",
            &(|_: &Target| Ok(false)) as &dyn Fn(&Target) -> Result<bool, Box<dyn Error>>,
        ),
        ("bsd", &detect_bsd),
        ("gettext", &have_gettext),
    ] {
        match handler(&target) {
            Err(e) => rsconf::warn!("{}: {}", feature, e),
            Ok(true) => rsconf::enable_feature(feature),
            Ok(false) => (),
        }
    }
}

fn compiles(file: &str) -> bool {
    let mut command = cc::Build::new()
        .flag("-fsyntax-only")
        .get_compiler()
        .to_command();
    command.arg(file);
    command.stdout(Stdio::null());
    command.stderr(Stdio::null());
    command.status().unwrap().success()
}

/// Detect if we're being compiled for a BSD-derived OS, allowing targeting code conditionally with
/// `#[cfg(feature = "bsd")]`.
///
/// Rust offers fine-grained conditional compilation per-os for the popular operating systems, but
/// doesn't necessarily include less-popular forks nor does it group them into families more
/// specific than "windows" vs "unix" so we can conditionally compile code for BSD systems.
fn detect_bsd(_: &Target) -> Result<bool, Box<dyn Error>> {
    // Instead of using `uname`, we can inspect the TARGET env variable set by Cargo. This lets us
    // support cross-compilation scenarios.
    let mut target = std::env::var("TARGET").unwrap();
    if !target.chars().all(|c| c.is_ascii_lowercase()) {
        target = target.to_ascii_lowercase();
    }
    let result = target.ends_with("bsd") || target.ends_with("dragonfly");
    #[cfg(any(
        target_os = "dragonfly",
        target_os = "freebsd",
        target_os = "netbsd",
        target_os = "openbsd",
    ))]
    assert!(result, "Target incorrectly detected as not BSD!");
    Ok(result)
}

/// Detect libintl/gettext and its needed symbols to enable internationalization/localization
/// support.
fn have_gettext(target: &Target) -> Result<bool, Box<dyn Error>> {
    // The following script correctly detects and links against gettext, but so long as we are using
    // C++ and generate a static library linked into the C++ binary via CMake, we need to account
    // for the CMake option WITH_GETTEXT being explicitly disabled.
    rsconf::rebuild_if_env_changed("CMAKE_WITH_GETTEXT");
    if let Some(with_gettext) = std::env::var_os("CMAKE_WITH_GETTEXT") {
        if with_gettext.eq_ignore_ascii_case("0") {
            return Ok(false);
        }
    }

    // In order for fish to correctly operate, we need some way of notifying libintl to invalidate
    // its localizations when the locale environment variables are modified. Without the libintl
    // symbol _nl_msg_cat_cntr, we cannot use gettext even if we find it.
    let mut libraries = Vec::new();
    let mut found = 0;
    let symbols = ["gettext", "_nl_msg_cat_cntr"];
    for symbol in &symbols {
        // Historically, libintl was required in order to use gettext() and co, but that
        // functionality was subsumed by some versions of libc.
        if target.has_symbol_in::<&str>(symbol, &[]) {
            // No need to link anything special for this symbol
            found += 1;
            continue;
        }
        for library in ["intl", "gettextlib"] {
            if target.has_symbol(symbol, library) {
                libraries.push(library);
                found += 1;
                continue;
            }
        }
    }
    match found {
        0 => Ok(false),
        1 => Err(format!("gettext found but cannot be used without {}", symbols[1]).into()),
        _ => {
            rsconf::link_libraries(&libraries, LinkType::Default);
            Ok(true)
        }
    }
}
