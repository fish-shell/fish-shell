use rsconf::{LinkType, Target};
use std::error::Error;

fn main() {
    cc::Build::new().file("src/compat.c").compile("libcompat.a");

    let rust_dir = std::env::var("CARGO_MANIFEST_DIR").expect("Env var CARGO_MANIFEST_DIR missing");
    let target_dir =
        std::env::var("FISH_RUST_TARGET_DIR").unwrap_or(format!("{}/{}", rust_dir, "target/"));
    let fish_src_dir = format!("{}/{}", rust_dir, "../src/");

    // Where cxx emits its header.
    let cxx_include_dir = format!("{}/{}", target_dir, "cxxbridge/rust/");

    // If FISH_BUILD_DIR is given by CMake, then use it; otherwise assume it's at ../build.
    let fish_build_dir =
        std::env::var("FISH_BUILD_DIR").unwrap_or(format!("{}/{}", rust_dir, "../build/"));

    // Where autocxx should put its stuff.
    let autocxx_gen_dir = std::env::var("FISH_AUTOCXX_GEN_DIR")
        .unwrap_or(format!("{}/{}", fish_build_dir, "fish-autocxx-gen/"));

    let mut build = cc::Build::new();
    // Add to the default library search path
    build.flag_if_supported("-L/usr/local/lib/");
    rsconf::add_library_search_path("/usr/local/lib");
    let mut detector = Target::new_from(build).unwrap();
    // Keep verbose mode on until we've ironed out rust build script stuff
    // Note that if autocxx fails to compile any rust code, you'll see the full and unredacted
    // stdout/stderr output, which will include things that LOOK LIKE compilation errors as rsconf
    // tries to build various test files to try and figure out which libraries and symbols are
    // available. IGNORE THESE and scroll to the very bottom of the build script output, past all
    // these errors, to see the actual issue.
    detector.set_verbose(true);
    detect_features(detector);

    // Emit cxx junk.
    // This allows "Rust to be used from C++"
    // This must come before autocxx so that cxx can emit its cxx.h header.
    let source_files = vec![
        "src/abbrs.rs",
        "src/ast.rs",
        "src/builtins/shared.rs",
        "src/common.rs",
        "src/env/env_ffi.rs",
        "src/env_dispatch.rs",
        "src/event.rs",
        "src/fd_monitor.rs",
        "src/fd_readable_set.rs",
        "src/fds.rs",
        "src/ffi_init.rs",
        "src/ffi_tests.rs",
        "src/fish_indent.rs",
        "src/future_feature_flags.rs",
        "src/highlight.rs",
        "src/job_group.rs",
        "src/kill.rs",
        "src/null_terminated_array.rs",
        "src/output.rs",
        "src/parse_constants.rs",
        "src/parse_tree.rs",
        "src/parse_util.rs",
        "src/print_help.rs",
        "src/redirection.rs",
        "src/signal.rs",
        "src/smoke.rs",
        "src/termsize.rs",
        "src/threads.rs",
        "src/timer.rs",
        "src/tokenizer.rs",
        "src/topic_monitor.rs",
        "src/trace.rs",
        "src/util.rs",
        "src/wait_handle.rs",
    ];
    cxx_build::bridges(&source_files)
        .flag_if_supported("-std=c++11")
        .include(&fish_src_dir)
        .include(&fish_build_dir) // For config.h
        .include(&cxx_include_dir) // For cxx.h
        .flag("-Wno-comment")
        .compile("fish-rust");

    // Emit autocxx junk.
    // This allows "C++ to be used from Rust."
    let include_paths = [&fish_src_dir, &fish_build_dir, &cxx_include_dir];
    let mut builder = autocxx_build::Builder::new("src/ffi.rs", include_paths);
    // Use autocxx's custom output directory unless we're being called by `rust-analyzer` and co.,
    // in which case stick to the default target directory so code intelligence continues to work.
    if std::env::var("RUSTC_WRAPPER").map_or(true, |wrapper| {
        !(wrapper.contains("rust-analyzer") || wrapper.contains("intellij-rust-native-helper"))
    }) {
        // We need this reassignment because of how the builder pattern works
        builder = builder.custom_gendir(autocxx_gen_dir.into());
    }
    let mut b = builder.build().unwrap();
    b.flag_if_supported("-std=c++11")
        .flag("-Wno-comment")
        .compile("fish-rust-autocxx");
    rsconf::rebuild_if_paths_changed(&source_files);
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
