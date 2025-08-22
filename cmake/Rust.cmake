include(FindRust)
find_package(Rust REQUIRED)

set(FISH_RUST_BUILD_DIR "${CMAKE_BINARY_DIR}/cargo/build")

if(DEFINED ASAN)
    list(APPEND CARGO_FLAGS "-Z" "build-std")
    list(APPEND FISH_CRATE_FEATURES "asan")
endif()
if(DEFINED TSAN)
    list(APPEND CARGO_FLAGS "-Z" "build-std")
    list(APPEND FISH_CRATE_FEATURES "tsan")
endif()

if (Rust_CARGO_TARGET)
    set(rust_target_dir "${FISH_RUST_BUILD_DIR}/${Rust_CARGO_TARGET}")
else()
    set(rust_target_dir "${FISH_RUST_BUILD_DIR}/${Rust_CARGO_HOST_TARGET}")
endif()

set(rust_profile $<IF:$<CONFIG:Debug>,debug,$<IF:$<CONFIG:RelWithDebInfo>,release-with-debug,release>>)
set(rust_debugflags "$<$<CONFIG:Debug>:-g>$<$<CONFIG:RelWithDebInfo>:-g>")


if(FISH_CRATE_FEATURES)
    set(FEATURES_ARG ${FISH_CRATE_FEATURES})
    list(PREPEND FEATURES_ARG "--features")
endif()

# Tell Cargo where our build directory is so it can find Cargo.toml.
set(VARS_FOR_CARGO
    "FISH_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "PREFIX=${CMAKE_INSTALL_PREFIX}"
    # Cheesy so we can tell cmake was used to build
    "CMAKE=1"
    "DOCDIR=${CMAKE_INSTALL_FULL_DOCDIR}"
    "DATADIR=${CMAKE_INSTALL_FULL_DATADIR}"
    "SYSCONFDIR=${CMAKE_INSTALL_FULL_SYSCONFDIR}"
    "BINDIR=${CMAKE_INSTALL_FULL_BINDIR}"
    "CARGO_TARGET_DIR=${FISH_RUST_BUILD_DIR}"
    "CARGO_BUILD_RUSTC=${Rust_COMPILER}"
    "${FISH_PCRE2_BUILDFLAG}"
    "RUSTFLAGS=$ENV{RUSTFLAGS} ${rust_debugflags}"
)
