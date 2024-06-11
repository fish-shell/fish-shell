# CMake 3.19 is needed for file(REAL_PATH)
cmake_minimum_required(VERSION 3.19)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(cmake/Mac.cmake)

project(fish LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DEFAULT_BUILD_TYPE "Debug")

# Generate Xcode schemas (but not for tests).
set(CMAKE_XCODE_GENERATE_SCHEME 1)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to default '${DEFAULT_BUILD_TYPE}'")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
endif()

# Set up standard directories.
include(GNUInstallDirs)
add_definitions(-D_UNICODE=1)

include(cmake/gettext.cmake)

# Set up PCRE2
# This sets an environment variable that needs to be available before the Rust stanzas
include(cmake/PCRE2.cmake)

include(cmake/Rust.cmake)

# Work around issue where archive-built libs go in the wrong place.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  set(FISH_IN_TREE_BUILD TRUE)
else()
  set(FISH_IN_TREE_BUILD FALSE)
endif()

# Set up the machinery around FISH-BUILD-VERSION-FILE
# This defines the FBVF variable.
include(Version)

# Let fish pick up when we're running out of the build directory without installing
get_filename_component(REAL_CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}" REALPATH)
get_filename_component(REAL_CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}" REALPATH)
add_definitions(-DCMAKE_BINARY_DIR="${REAL_CMAKE_BINARY_DIR}")
add_definitions(-DCMAKE_SOURCE_DIR="${REAL_CMAKE_SOURCE_DIR}")


# Define a function to link dependencies.
function(FISH_LINK_DEPS_AND_SIGN target)
  add_custom_target(
    ${target} ALL
    COMMAND
      "${CMAKE_COMMAND}" -E
        env ${VARS_FOR_CARGO}
          ${Rust_CARGO}
            build --bin ${target}
            $<$<CONFIG:Release,RelWithDebInfo>:--release>
            --target ${Rust_CARGO_TARGET}
            ${CARGO_FLAGS}
            ${FEATURES_ARG}
      &&
      "${CMAKE_COMMAND}" -E
        copy "${rust_target_dir}/${rust_profile}/${target}" "${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
  )
  codesign_on_mac(${target})
endfunction(FISH_LINK_DEPS_AND_SIGN)

# Define fish.
fish_link_deps_and_sign(fish)

# Define fish_indent.
fish_link_deps_and_sign(fish_indent)

# Define fish_key_reader.
fish_link_deps_and_sign(fish_key_reader)

# Set up the docs.
include(cmake/Docs.cmake)

# A helper for running tests.
add_executable(fish_test_helper src/fish_test_helper.c)
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
