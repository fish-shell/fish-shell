fn main() -> miette::Result<()> {
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

    // Emit cxx junk.
    // This allows "Rust to be used from C++"
    // This must come before autocxx so that cxx can emit its cxx.h header.
    let source_files = vec![
        "src/fd_readable_set.rs",
        "src/ffi_init.rs",
        "src/ffi_tests.rs",
        "src/future_feature_flags.rs",
        "src/parse_constants.rs",
        "src/redirection.rs",
        "src/smoke.rs",
        "src/timer.rs",
        "src/tokenizer.rs",
        "src/topic_monitor.rs",
        "src/util.rs",
        "src/builtins/shared.rs",
    ];
    cxx_build::bridges(&source_files)
        .flag_if_supported("-std=c++11")
        .include(&fish_src_dir)
        .include(&fish_build_dir) // For config.h
        .include(&cxx_include_dir) // For cxx.h
        .compile("fish-rust");

    // Emit autocxx junk.
    // This allows "C++ to be used from Rust."
    let include_paths = [&fish_src_dir, &fish_build_dir, &cxx_include_dir];
    let mut b = autocxx_build::Builder::new("src/ffi.rs", include_paths)
        .custom_gendir(autocxx_gen_dir.into())
        .build()?;
    b.flag_if_supported("-std=c++11")
        .compile("fish-rust-autocxx");
    for file in source_files {
        println!("cargo:rerun-if-changed={file}");
    }

    Ok(())
}
