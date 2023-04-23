fn main() -> miette::Result<()> {
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

    detect_features();

    // Emit cxx junk.
    // This allows "Rust to be used from C++"
    // This must come before autocxx so that cxx can emit its cxx.h header.
    let source_files = vec![
        "src/abbrs.rs",
        "src/ast.rs",
        "src/event.rs",
        "src/common.rs",
        "src/fd_monitor.rs",
        "src/fd_readable_set.rs",
        "src/fds.rs",
        "src/ffi_init.rs",
        "src/ffi_tests.rs",
        "src/fish_indent.rs",
        "src/future_feature_flags.rs",
        "src/highlight.rs",
        "src/job_group.rs",
        "src/null_terminated_array.rs",
        "src/parse_constants.rs",
        "src/parse_tree.rs",
        "src/parse_util.rs",
        "src/redirection.rs",
        "src/signal.rs",
        "src/smoke.rs",
        "src/termsize.rs",
        "src/timer.rs",
        "src/tokenizer.rs",
        "src/topic_monitor.rs",
        "src/threads.rs",
        "src/trace.rs",
        "src/util.rs",
        "src/wait_handle.rs",
        "src/builtins/shared.rs",
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
    let mut b = builder.build()?;
    b.flag_if_supported("-std=c++11")
        .flag("-Wno-comment")
        .compile("fish-rust-autocxx");
    for file in source_files {
        println!("cargo:rerun-if-changed={file}");
    }

    Ok(())
}

/// Dynamically enables certain features at build-time, without their having to be explicitly
/// enabled in the `cargo build --features xxx` invocation.
///
/// This can be used to enable features that we check for and conditionally compile according to in
/// our own codebase, but [can't be used to pull in dependencies](0) even if they're gated (in
/// `Cargo.toml`) behind a feature we just enabled.
///
/// [0]: https://github.com/rust-lang/cargo/issues/5499
fn detect_features() {
    for (feature, detector) in [
        // Ignore the first line, it just sets up the type inference. Model new entries after the
        // second line.
        ("", &(|| Ok(false)) as &dyn Fn() -> miette::Result<bool>),
        ("bsd", &detect_bsd),
    ] {
        match detector() {
            Err(e) => eprintln!("ERROR: {feature} detect: {e}"),
            Ok(true) => println!("cargo:rustc-cfg=feature=\"{feature}\""),
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
fn detect_bsd() -> miette::Result<bool> {
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
