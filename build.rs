#![allow(clippy::uninlined_format_args)]

use rsconf::{LinkType, Target};
use std::env;
use std::error::Error;
use std::path::{Path, PathBuf};

fn main() {
    setup_paths();

    // Add our default to enable tools that don't go through CMake, like "cargo test" and the
    // language server.

    // FISH_BUILD_DIR is set by CMake, if we are using it.
    // OUT_DIR is set by Cargo when the build script is running (not compiling)
    let default_build_dir = env::var("OUT_DIR").unwrap();
    let build_dir = option_env!("FISH_BUILD_DIR").unwrap_or(&default_build_dir);
    let build_dir = std::fs::canonicalize(build_dir).unwrap();
    let build_dir = build_dir.to_str().unwrap();
    rsconf::set_env_value("FISH_BUILD_DIR", build_dir);
    // We need to canonicalize (i.e. realpath) the manifest dir because we want to be able to
    // compare it directly as a string at runtime.
    rsconf::set_env_value(
        "CARGO_MANIFEST_DIR",
        std::fs::canonicalize(env!("CARGO_MANIFEST_DIR"))
            .unwrap()
            .as_path()
            .to_str()
            .unwrap(),
    );

    // Per https://doc.rust-lang.org/cargo/reference/build-scripts.html#inputs-to-the-build-script,
    // the source directory is the current working directory of the build script
    rsconf::set_env_value(
        "FISH_BUILD_VERSION",
        &get_version(&env::current_dir().unwrap()),
    );

    rsconf::rebuild_if_path_changed("src/libc.c");
    cc::Build::new()
        .file("src/libc.c")
        .include(build_dir)
        .compile("flibc.a");

    let mut build = cc::Build::new();
    // Add to the default library search path
    build.flag_if_supported("-L/usr/local/lib/");
    rsconf::add_library_search_path("/usr/local/lib");
    let mut target = Target::new_from(build).unwrap();
    // Keep verbose mode on until we've ironed out rust build script stuff
    target.set_verbose(true);
    detect_cfgs(&mut target);
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
fn detect_cfgs(target: &mut Target) {
    for (name, handler) in [
        // Ignore the first entry, it just sets up the type inference. Model new entries after the
        // second line.
        (
            "",
            &(|_: &Target| Ok(false)) as &dyn Fn(&Target) -> Result<bool, Box<dyn Error>>,
        ),
        ("bsd", &detect_bsd),
        ("gettext", &have_gettext),
        ("small_main_stack", &has_small_stack),
        // See if libc supports the thread-safe localeconv_l(3) alternative to localeconv(3).
        ("localeconv_l", &|target| {
            Ok(target.has_symbol("localeconv_l"))
        }),
        ("FISH_USE_POSIX_SPAWN", &|target| {
            Ok(target.has_header("spawn.h"))
        }),
        ("HAVE_PIPE2", &|target| {
            Ok(target.has_symbol("pipe2"))
        }),
        ("HAVE_EVENTFD", &|target| {
            // FIXME: NetBSD 10 has eventfd, but the libc crate does not expose it.
            if cfg!(target_os = "netbsd") {
                 Ok(false)
             } else {
                 Ok(target.has_header("sys/eventfd.h"))
            }
        }),
        ("HAVE_WAITSTATUS_SIGNAL_RET", &|target| {
            Ok(target.r#if("WEXITSTATUS(0x007f) == 0x7f", &["sys/wait.h"]))
        }),
    ] {
        match handler(target) {
            Err(e) => {
                rsconf::warn!("{}: {}", name, e);
                rsconf::declare_cfg(name, false);
            },
            Ok(enabled) => rsconf::declare_cfg(name, enabled),
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
    let is_bsd = target.ends_with("bsd") || target.ends_with("dragonfly");
    #[cfg(any(
        target_os = "dragonfly",
        target_os = "freebsd",
        target_os = "netbsd",
        target_os = "openbsd",
    ))]
    assert!(is_bsd, "Target incorrectly detected as not BSD!");
    Ok(is_bsd)
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
        if target.has_symbol(symbol) {
            // No need to link anything special for this symbol
            found += 1;
            continue;
        }
        for library in ["intl", "gettextlib"] {
            if target.has_symbol_in(symbol, &[library]) {
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

/// Rust sets the stack size of newly created threads to a sane value, but is at at the mercy of the
/// OS when it comes to the size of the main stack. Some platforms we support default to a tiny
/// 0.5 MiB main stack, which is insufficient for fish's MAX_EVAL_DEPTH/MAX_STACK_DEPTH values.
///
/// 0.5 MiB is small enough that we'd have to drastically reduce MAX_STACK_DEPTH to less than 10, so
/// we instead use a workaround to increase the main thread size.
fn has_small_stack(_: &Target) -> Result<bool, Box<dyn Error>> {
    #[cfg(not(any(target_os = "macos", target_os = "netbsd")))]
    return Ok(false);

    // NetBSD 10 also needs this but can't find pthread_get_stacksize_np.
    #[cfg(target_os = "netbsd")]
    return Ok(true);

    #[cfg(target_os = "macos")]
    {
        use core::ffi;

        extern "C" {
            fn pthread_get_stacksize_np(thread: *const ffi::c_void) -> usize;
            fn pthread_self() -> *const ffi::c_void;
        }

        // build.rs is executed on the main thread, so we are getting the main thread's stack size.
        // Modern macOS versions default to an 8 MiB main stack but legacy OS X have a 0.5 MiB one.
        let stack_size = unsafe { pthread_get_stacksize_np(pthread_self()) };
        const TWO_MIB: usize = 2 * 1024 * 1024 - 1;
        match stack_size {
            0..=TWO_MIB => Ok(true),
            _ => Ok(false),
        }
    }
}

fn setup_paths() {
    fn get_path(name: &str, default: &str, onvar: PathBuf) -> PathBuf {
        let mut var = PathBuf::from(env::var(name).unwrap_or(default.to_string()));
        if var.is_relative() {
            var = onvar.join(var);
        }
        var
    }

    let prefix = PathBuf::from(env::var("PREFIX").unwrap_or("/usr/local".to_string()));
    if prefix.is_relative() {
        panic!("Can't have relative prefix");
    }
    rsconf::rebuild_if_env_changed("PREFIX");
    rsconf::set_env_value("PREFIX", prefix.to_str().unwrap());

    let datadir = get_path("DATADIR", "share/", prefix.clone());
    rsconf::set_env_value("DATADIR", datadir.to_str().unwrap());
    rsconf::rebuild_if_env_changed("DATADIR");

    let bindir = get_path("BINDIR", "bin/", prefix.clone());
    rsconf::set_env_value("BINDIR", bindir.to_str().unwrap());
    rsconf::rebuild_if_env_changed("BINDIR");

    let sysconfdir = get_path("SYSCONFDIR", "etc/", datadir.clone());
    rsconf::set_env_value("SYSCONFDIR", sysconfdir.to_str().unwrap());
    rsconf::rebuild_if_env_changed("SYSCONFDIR");

    let localedir = get_path("LOCALEDIR", "locale/", datadir.clone());
    rsconf::set_env_value("LOCALEDIR", localedir.to_str().unwrap());
    rsconf::rebuild_if_env_changed("LOCALEDIR");

    let docdir = get_path("DOCDIR", "doc/fish", datadir.clone());
    rsconf::set_env_value("DOCDIR", docdir.to_str().unwrap());
    rsconf::rebuild_if_env_changed("DOCDIR");
}

fn get_version(src_dir: &Path) -> String {
    use std::fs::read_to_string;
    use std::process::Command;

    if let Ok(var) = std::env::var("FISH_BUILD_VERSION") {
        return var;
    }

    let path = PathBuf::from(src_dir).join("version");
    if let Ok(strver) = read_to_string(path) {
        return strver.to_string();
    }

    let args = &["describe", "--always", "--dirty=-dirty"];
    if let Ok(output) = Command::new("git").args(args).output() {
        let rev = String::from_utf8_lossy(&output.stdout).trim().to_string();
        if !rev.is_empty() {
            return rev;
        }
    }

    "unknown".to_string()
}
