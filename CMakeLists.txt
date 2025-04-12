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

include(cmake/gettext.cmake)

# Set up PCRE2
# This sets an environment variable that needs to be available before the Rust stanzas
include(cmake/PCRE2.cmake)

include(cmake/Rust.cmake)

# Work around issue where archive-built libs go in the wrong place.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# Set up the machinery around FISH-BUILD-VERSION-FILE
# This defines the FBVF variable.
include(Version)

# Let fish pick up when we're running out of the build directory without installing
get_filename_component(REAL_CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}" REALPATH)
get_filename_component(REAL_CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}" REALPATH)
add_definitions(-DCMAKE_BINARY_DIR="${REAL_CMAKE_BINARY_DIR}")
add_definitions(-DCMAKE_SOURCE_DIR="${REAL_CMAKE_SOURCE_DIR}")

set(build_types Release RelWithDebInfo Debug "")
if(NOT "${CMAKE_BUILD_TYPE}" IN_LIST build_types)
  message(WARNING "Unsupported build type ${CMAKE_BUILD_TYPE}. If this doesn't build, try one of Release, RelWithDebInfo or Debug")
endif()

# Define a function to build and link dependencies.
function(CREATE_TARGET target)
  add_custom_target(
    ${target} ALL
    COMMAND
      "${CMAKE_COMMAND}" -E
        env ${VARS_FOR_CARGO}
          ${Rust_CARGO}
            build --bin ${target}
            $<$<CONFIG:Release>:--release>
            $<$<CONFIG:RelWithDebInfo>:--profile=release-with-debug>
            --target ${Rust_CARGO_TARGET}
            --no-default-features
            ${CARGO_FLAGS}
            ${FEATURES_ARG}
      &&
      "${CMAKE_COMMAND}" -E
        copy "${rust_target_dir}/${rust_profile}/${target}" "${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
  )
endfunction(CREATE_TARGET)

# Define fish.
create_target(fish)

# Define fish_indent.
create_target(fish_indent)

# Define fish_key_reader.
create_target(fish_key_reader)

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
