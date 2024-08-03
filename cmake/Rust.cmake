# Trying to build using the resolved toolchain causes all kinds of weird errors
# Just let rustup do its job
set(Rust_RESOLVE_RUSTUP_TOOLCHAINS Off)

include(FindRust)
find_package(Rust REQUIRED)

set(FISH_RUST_BUILD_DIR "${CMAKE_BINARY_DIR}/cargo/build")

if(DEFINED ASAN)
    list(APPEND CARGO_FLAGS "-Z" "build-std")
    list(APPEND FISH_CRATE_FEATURES "asan")
endif()

if (Rust_CARGO_TARGET)
    set(rust_target_dir "${FISH_RUST_BUILD_DIR}/${Rust_CARGO_TARGET}")
else()
    set(rust_target_dir "${FISH_RUST_BUILD_DIR}/${Rust_CARGO_HOST_TARGET}")
endif()

set(rust_profile $<IF:$<CONFIG:Debug>,debug,release>)
set(rust_debugflags "$<$<CONFIG:Debug,RelWithDebInfo>:-g>")


# Temporary hack to propogate CMake flags/options to build.rs. We need to get CMake to evaluate the
# truthiness of the strings if they are set.
set(CMAKE_WITH_GETTEXT "1")
if(DEFINED WITH_GETTEXT AND NOT "${WITH_GETTEXT}")
    set(CMAKE_WITH_GETTEXT "0")
endif()

# CMAKE_BINARY_DIR can include symlinks, since we want to compare this to the dir fish is executed in we need to canonicalize it.
file(REAL_PATH "${CMAKE_BINARY_DIR}" fish_binary_dir)

if(FISH_CRATE_FEATURES)
    set(FEATURES_ARG ${FISH_CRATE_FEATURES})
    list(PREPEND FEATURES_ARG "--features")
endif()

get_property(
    RUSTC_EXECUTABLE
    TARGET Rust::Rustc PROPERTY IMPORTED_LOCATION
)

# Tell Cargo where our build directory is so it can find Cargo.toml.
set(VARS_FOR_CARGO
    "FISH_BUILD_DIR=${fish_binary_dir}"
    "PREFIX=${CMAKE_INSTALL_PREFIX}"
    # Temporary hack to propogate CMake flags/options to build.rs.
    "CMAKE_WITH_GETTEXT=${CMAKE_WITH_GETTEXT}"
    "DOCDIR=${CMAKE_INSTALL_FULL_DOCDIR}"
    "DATADIR=${CMAKE_INSTALL_FULL_DATADIR}"
    "SYSCONFDIR=${CMAKE_INSTALL_FULL_SYSCONFDIR}"
    "BINDIR=${CMAKE_INSTALL_FULL_BINDIR}"
    "LOCALEDIR=${CMAKE_INSTALL_FULL_LOCALEDIR}"
    "CARGO_TARGET_DIR=${FISH_RUST_BUILD_DIR}"
    "CARGO_BUILD_RUSTC=${RUSTC_EXECUTABLE}"
    "${FISH_PCRE2_BUILDFLAG}"
    "RUSTFLAGS=$ENV{RUSTFLAGS} ${rust_debugflags}"
)
