cmake_minimum_required(VERSION 3.15)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

project(fish LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DEFAULT_BUILD_TYPE "RelWithDebInfo")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to default '${DEFAULT_BUILD_TYPE}'")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
endif()

# Set up standard directories.
include(GNUInstallDirs)

# Set up PCRE2
# This sets an environment variable that needs to be available before the Rust stanzas
include(cmake/PCRE2.cmake)

include(cmake/Rust.cmake)

# Work around issue where archive-built libs go in the wrong place.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

find_program(SPHINX_EXECUTABLE NAMES sphinx-build
    HINTS
    $ENV{SPHINX_DIR}
    PATH_SUFFIXES bin
    DOC "Sphinx documentation generator")


# Tell Cargo where our build directory is so it can find Cargo.toml.
set(VARS_FOR_CARGO
    "FISH_CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}"
    "PREFIX=${CMAKE_INSTALL_PREFIX}"
    "DOCDIR=${CMAKE_INSTALL_FULL_DOCDIR}"
    "DATADIR=${CMAKE_INSTALL_FULL_DATADIR}"
    "SYSCONFDIR=${CMAKE_INSTALL_FULL_SYSCONFDIR}"
    "BINDIR=${CMAKE_INSTALL_FULL_BINDIR}"
    "CARGO_TARGET_DIR=${FISH_RUST_BUILD_DIR}"
    "CARGO_BUILD_RUSTC=${Rust_COMPILER}"
    "${FISH_PCRE2_BUILDFLAG}"
    "FISH_SPHINX=${SPHINX_EXECUTABLE}"
)

# Let fish pick up when we're running out of the build directory without installing
get_filename_component(REAL_CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}" REALPATH)
get_filename_component(REAL_CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}" REALPATH)
add_definitions(-DCMAKE_BINARY_DIR="${REAL_CMAKE_BINARY_DIR}")
add_definitions(-DCMAKE_SOURCE_DIR="${REAL_CMAKE_SOURCE_DIR}")

set(build_types Release RelWithDebInfo Debug "")
if(NOT "${CMAKE_BUILD_TYPE}" IN_LIST build_types)
  message(WARNING "Unsupported build type ${CMAKE_BUILD_TYPE}. If this doesn't build, try one of Release, RelWithDebInfo or Debug")
endif()

add_custom_target(
  fish ALL
  COMMAND
    "${CMAKE_COMMAND}" -E
      env ${VARS_FOR_CARGO}
        ${Rust_CARGO}
          build --bin fish
          $<$<CONFIG:Release>:--release>
          $<$<CONFIG:RelWithDebInfo>:--profile=release-with-debug>
          --target ${Rust_CARGO_TARGET}
          --no-default-features
          --features=${FISH_CARGO_FEATURES}
          ${CARGO_FLAGS}
    &&
    "${CMAKE_COMMAND}" -E
      copy "${rust_target_dir}/${rust_profile}/fish" "${CMAKE_CURRENT_BINARY_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
)

function(CREATE_LINK target)
  add_custom_target(
    ${target} ALL
    DEPENDS fish
    COMMAND ln -f fish ${target}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )
endfunction(CREATE_LINK)

# Define fish_indent.
create_link(fish_indent)

# Define fish_key_reader.
create_link(fish_key_reader)

# Set up the docs.
include(cmake/Docs.cmake)

# Set up tests.
include(cmake/Tests.cmake)

# Benchmarking support.
include(cmake/Benchmark.cmake)

# Set up install.
include(cmake/Install.cmake)

# Mac app.
include(cmake/MacApp.cmake)

include(FeatureSummary)
feature_summary(WHAT ALL)
