#![allow(clippy::uninlined_format_args)]

use rsconf::{LinkType, Target};
use std::env;
use std::error::Error;
use std::path::{Path, PathBuf};

fn main() {
    setup_paths();

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
    if !curses_libnames.is_empty() {
        rsconf::link_libraries(&curses_libnames, LinkType::Default);
    } else {
        rsconf::link_libraries(&["ncurses"], LinkType::Default);
    }

    cc::Build::new()
        .file("src/libc.c")
        .include(&build_dir)
        .compile("flibc.a");

    let mut build = cc::Build::new();
    // Add to the default library search path
    build.flag_if_supported("-L/usr/local/lib/");
    rsconf::add_library_search_path("/usr/local/lib");
    let mut detector = Target::new_from(build).unwrap();
    // Keep verbose mode on until we've ironed out rust build script stuff
    detector.set_verbose(true);
    detect_cfgs(detector);
}

/// Check target system support for certain functionality dynamically when the build is invoked,
/// without their having to be explicitly enabled in the `cargo build --features xxx` invocation.
///
/// We are using [`rsconf::enable_cfg()`] instead of [`rsconf::enable_feature()`] as rust features
/// should be used for things that a user can/would reasonably enable or disable to tweak or coerce
/// behavior, but here we are testing for whether or not things are supported altogether.
///
/// This can be used to enable features that we check for and conditionally compile according to in
/// our own codebase, but [can't be used to pull in dependencies](0) even if they're gated (in
/// `Cargo.toml`) behind a feature we just enabled.
///
/// [0]: https://github.com/rust-lang/cargo/issues/5499
#[rustfmt::skip]
fn detect_cfgs(target: Target) {
    for (name, handler) in [
        // Ignore the first entry, it just sets up the type inference. Model new entries after the
        // second line.
        (
            "",
            &(|_: &Target| Ok(false)) as &dyn Fn(&Target) -> Result<bool, Box<dyn Error>>,
        ),
        ("bsd", &detect_bsd),
        ("gettext", &have_gettext),
        // See if libc supports the thread-safe localeconv_l(3) alternative to localeconv(3).
        ("localeconv_l", &|target| {
            Ok(target.has_symbol("localeconv_l", None))
        }),
        ("FISH_USE_POSIX_SPAWN", &|target| {
            Ok(target.has_header("spawn.h"))
        }),
        ("HAVE_PIPE2", &|target| {
            Ok(target.has_symbol("pipe2", None))
        }),
        ("HAVE_EVENTFD", &|target| {
            Ok(target.has_header("sys/eventfd.h"))
        }),
        ("HAVE_WAITSTATUS_SIGNAL_RET", &|target| {
            Ok(target.r#if("WEXITSTATUS(0x007f) == 0x7f", "sys/wait.h"))
        }),
    ] {
        match handler(&target) {
            Err(e) => rsconf::warn!("{}: {}", name, e),
            Ok(true) => rsconf::enable_cfg(name),
            Ok(false) => (),
        }
    }
}

/// Detect if we're being compiled for a BSD-derived OS, allowing targeting code conditionally with
/// `#[cfg(bsd)]`.
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

fn get_path(name: &str, default: &str, onvar: PathBuf) -> PathBuf {
    let mut var = PathBuf::from(env::var(name).unwrap_or(default.to_string()));
    if var.is_relative() {
        var = onvar.join(var);
    }
    var
}

fn setup_paths() {
    rsconf::rebuild_if_env_changed("PREFIX");
    rsconf::rebuild_if_env_changed("DATADIR");
    rsconf::rebuild_if_env_changed("BINDIR");
    rsconf::rebuild_if_env_changed("SYSCONFDIR");
    rsconf::rebuild_if_env_changed("LOCALEDIR");
    rsconf::rebuild_if_env_changed("DOCDIR");
    let prefix = PathBuf::from(env::var("PREFIX").unwrap_or("/usr/local".to_string()));
    if prefix.is_relative() {
        panic!("Can't have relative prefix");
    }
    println!("cargo:rustc-env=PREFIX={}", prefix.to_str().unwrap());

    let datadir = get_path("DATADIR", "share/", prefix.clone());
    println!("cargo:rustc-env=DATADIR={}", datadir.to_str().unwrap());
    let bindir = get_path("BINDIR", "bin/", prefix.clone());
    println!("cargo:rustc-env=BINDIR={}", bindir.to_str().unwrap());
    let sysconfdir = get_path("SYSCONFDIR", "etc/", datadir.clone());
    println!(
        "cargo:rustc-env=SYSCONFDIR={}",
        sysconfdir.to_str().unwrap()
    );
    let localedir = get_path("LOCALEDIR", "locale/", datadir.clone());
    println!("cargo:rustc-env=LOCALEDIR={}", localedir.to_str().unwrap());
    let docdir = get_path("DOCDIR", "doc/fish", datadir.clone());
    println!("cargo:rustc-env=DOCDIR={}", docdir.to_str().unwrap());
}
