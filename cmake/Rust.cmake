if(EXISTS "${CMAKE_SOURCE_DIR}/corrosion-vendor/")
    add_subdirectory("${CMAKE_SOURCE_DIR}/corrosion-vendor/")
else()
    include(FetchContent)

    # Don't let Corrosion's tests interfere with ours.
    set(CORROSION_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/mqudsi/corrosion
        GIT_TAG fish
    )

    FetchContent_MakeAvailable(Corrosion)

    add_custom_target(corrosion-vendor.tar.gz
        COMMAND git archive --format tar.gz --output "${CMAKE_BINARY_DIR}/corrosion-vendor.tar.gz"
        --prefix corrosion-vendor/ HEAD
        WORKING_DIRECTORY ${corrosion_SOURCE_DIR}
    )
endif()

set(fish_rust_target "fish-rust")

set(fish_autocxx_gen_dir "${CMAKE_BINARY_DIR}/fish-autocxx-gen/")

set(FISH_CRATE_FEATURES "fish-ffi-tests")
if(NOT DEFINED CARGO_FLAGS)
    # Corrosion doesn't like an empty string as FLAGS. This is basically a no-op alternative.
    # See https://github.com/corrosion-rs/corrosion/issues/356
    set(CARGO_FLAGS "--config" "foo=0")
endif()
if(DEFINED ASAN)
    list(APPEND CARGO_FLAGS "-Z" "build-std")
    list(APPEND FISH_CRATE_FEATURES "asan")
endif()

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_SOURCE_DIR}/Cargo.toml"
    CRATES "fish-rust"
    FEATURES "${FISH_CRATE_FEATURES}"
    FLAGS "${CARGO_FLAGS}"
)

# We need the build dir because cxx puts our headers in there.
# Corrosion doesn't expose the build dir, so poke where we shouldn't.
if (Rust_CARGO_TARGET)
    set(rust_target_dir "${CMAKE_BINARY_DIR}/cargo/build/${_CORROSION_RUST_CARGO_TARGET}")
else()
    set(rust_target_dir "${CMAKE_BINARY_DIR}/cargo/build/${_CORROSION_RUST_CARGO_HOST_TARGET}")
    corrosion_set_hostbuild(${fish_rust_target})
endif()

# Temporary hack to propogate CMake flags/options to build.rs. We need to get CMake to evaluate the
# truthiness of the strings if they are set.
set(CMAKE_WITH_GETTEXT "1")
if(DEFINED WITH_GETTEXT AND NOT "${WITH_GETTEXT}")
    set(CMAKE_WITH_GETTEXT "0")
endif()

# Tell Cargo where our build directory is so it can find config.h.
corrosion_set_env_vars(${fish_rust_target}
    "FISH_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "FISH_AUTOCXX_GEN_DIR=${fish_autocxx_gen_dir}"
    "FISH_RUST_TARGET_DIR=${rust_target_dir}"
    "PREFIX=${CMAKE_INSTALL_PREFIX}"
    # Temporary hack to propogate CMake flags/options to build.rs.
    "CMAKE_WITH_GETTEXT=${CMAKE_WITH_GETTEXT}"
)

target_include_directories(${fish_rust_target} INTERFACE
    "${rust_target_dir}/cxxbridge/${fish_rust_target}/src/"
    "${fish_autocxx_gen_dir}/include/"
)

# Tell fish what extra C++ files to compile.
define_property(
    TARGET PROPERTY fish_extra_cpp_files
    BRIEF_DOCS "Extra C++ files to compile for fish."
    FULL_DOCS "Extra C++ files to compile for fish."
)

set_property(TARGET ${fish_rust_target} PROPERTY fish_extra_cpp_files
    "${fish_autocxx_gen_dir}/cxx/gen0.cxx"
)
