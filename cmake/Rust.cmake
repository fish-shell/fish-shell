if(EXISTS "${CMAKE_SOURCE_DIR}/corrosion-vendor/")
    add_subdirectory("${CMAKE_SOURCE_DIR}/corrosion-vendor/")
else()
    include(FetchContent)

    # Don't let Corrosion's tests interfere with ours.
    set(CORROSION_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/mqudsi/corrosion
        GIT_TAG ghoti
    )

    FetchContent_MakeAvailable(Corrosion)

    add_custom_target(corrosion-vendor.tar.gz
        COMMAND git archive --format tar.gz --output "${CMAKE_BINARY_DIR}/corrosion-vendor.tar.gz"
        --prefix corrosion-vendor/ HEAD
        WORKING_DIRECTORY ${corrosion_SOURCE_DIR}
    )
endif()

set(ghoti_rust_target "ghoti-rust")

set(ghoti_autocxx_gen_dir "${CMAKE_BINARY_DIR}/ghoti-autocxx-gen/")

if(NOT DEFINED CARGO_FLAGS)
    # Corrosion doesn't like an empty string as FLAGS. This is basically a no-op alternative.
    # See https://github.com/corrosion-rs/corrosion/issues/356
    set(CARGO_FLAGS "--config" "foo=0")
endif()
if(DEFINED ASAN)
    list(APPEND CARGO_FLAGS "-Z" "build-std")
endif()

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_SOURCE_DIR}/ghoti-rust/Cargo.toml"
    FEATURES "ghoti-ffi-tests"
    FLAGS "${CARGO_FLAGS}"
)

# We need the build dir because cxx puts our headers in there.
# Corrosion doesn't expose the build dir, so poke where we shouldn't.
if (Rust_CARGO_TARGET)
    set(rust_target_dir "${CMAKE_BINARY_DIR}/cargo/build/${_CORROSION_RUST_CARGO_TARGET}")
else()
    set(rust_target_dir "${CMAKE_BINARY_DIR}/cargo/build/${_CORROSION_RUST_CARGO_HOST_TARGET}")
    corrosion_set_hostbuild(${ghoti_rust_target})
endif()

# Tell Cargo where our build directory is so it can find config.h.
corrosion_set_env_vars(${ghoti_rust_target} "FISH_BUILD_DIR=${CMAKE_BINARY_DIR}" "FISH_AUTOCXX_GEN_DIR=${ghoti_autocxx_gen_dir}" "FISH_RUST_TARGET_DIR=${rust_target_dir}")

target_include_directories(${ghoti_rust_target} INTERFACE
    "${rust_target_dir}/cxxbridge/${ghoti_rust_target}/src/"
    "${ghoti_autocxx_gen_dir}/include/"
)

# Tell ghoti what extra C++ files to compile.
define_property(
    TARGET PROPERTY ghoti_extra_cpp_files
    BRIEF_DOCS "Extra C++ files to compile for ghoti."
    FULL_DOCS "Extra C++ files to compile for ghoti."
)

set_property(TARGET ${ghoti_rust_target} PROPERTY ghoti_extra_cpp_files
    "${ghoti_autocxx_gen_dir}/cxx/gen0.cxx"
)
