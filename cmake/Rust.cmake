include(FetchContent)

# Don't let Corrosion's tests interfere with ours.
set(CORROSION_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/mqudsi/corrosion
    GIT_TAG fish
)

FetchContent_MakeAvailable(Corrosion)

set(fish_rust_target "fish-rust")

set(fish_autocxx_gen_dir "${CMAKE_BINARY_DIR}/fish-autocxx-gen/")

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_SOURCE_DIR}/fish-rust/Cargo.toml"
    FEATURES "fish-ffi-tests"
)

# We need the build dir because cxx puts our headers in there.
# Corrosion doesn't expose the build dir, so poke where we shouldn't.
if (Rust_CARGO_TARGET)
    set(rust_target_dir "${CMAKE_BINARY_DIR}/cargo/build/${_CORROSION_RUST_CARGO_TARGET}")
else()
    set(rust_target_dir "${CMAKE_BINARY_DIR}/cargo/build/${_CORROSION_RUST_CARGO_HOST_TARGET}")
    corrosion_set_hostbuild(${fish_rust_target})
endif()

# Tell Cargo where our build directory is so it can find config.h.
corrosion_set_env_vars(${fish_rust_target} "FISH_BUILD_DIR=${CMAKE_BINARY_DIR}" "FISH_AUTOCXX_GEN_DIR=${fish_autocxx_gen_dir}" "FISH_RUST_TARGET_DIR=${rust_target_dir}")

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
