cmake_minimum_required(VERSION 3.5)

if(POLICY CMP0066)
  cmake_policy(SET CMP0066 OLD)
endif()
if(POLICY CMP0067)
  cmake_policy(SET CMP0067 NEW)
endif()

include(cmake/Mac.cmake)

project(fish)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# We are C++11.
set(CMAKE_CXX_STANDARD 11)
set(DEFAULT_BUILD_TYPE "RelWithDebInfo")

# Generate Xcode schemas (but not for tests).
set(CMAKE_XCODE_GENERATE_SCHEME 1)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to default '${DEFAULT_BUILD_TYPE}'")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
endif()

# Set up standard directories.
include(GNUInstallDirs)
add_definitions(-D_UNICODE=1)

include(cmake/ConfigureChecks.cmake)
include(cmake/gettext.cmake)

include(cmake/Rust.cmake)

# Error out when linking statically, it doesn't work.
if (CMAKE_EXE_LINKER_FLAGS MATCHES ".*-static.*")
    message(FATAL_ERROR "Fish does not support static linking")
endif()

# Force colored warnings in Ninja's output, if the compiler has -fdiagnostics-color support.
# Rationale in https://github.com/ninja-build/ninja/issues/814
if (CMAKE_GENERATOR STREQUAL "Ninja" AND
    ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9) OR
     (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5) OR
     (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)))
      add_compile_options(-fdiagnostics-color=always)
endif()

# Enable a whole bunch of warnings, but turn off:
# - comment because we use a bunch of those, and they're not really all that harmful.
# - address, because that occurs for our mkostemp check (weak-linking requires us to compare `&mkostemp == nullptr`).
add_compile_options(-Wall -Wextra -Wno-comment -Wno-address)

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
  add_compile_options(-Wunused-template -Wunused-local-typedef -Wunused-macros)
endif()

# Disable exception handling.
add_compile_options(-fno-exceptions)

# Undefine NDEBUG to keep assert() in release builds.
add_definitions(-UNDEBUG)

# Enable large files on GNU.
add_definitions(-D_LARGEFILE_SOURCE
                -D_LARGEFILE64_SOURCE
                -D_FILE_OFFSET_BITS=64
                -D_ATFILE_SOURCE)

# Hide the CMake Rules directories in Xcode projects.
source_group("CMake Rules" REGULAR_EXPRESSION "^$")

# Put source and header files at top level under targets.
source_group("Source Files" REGULAR_EXPRESSION ".*\\.cpp")
source_group("Header Files" REGULAR_EXPRESSION ".*\\.h")
source_group("Builtins" "builtins/")

# Support folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Work around issue where archive-built libs go in the wrong place.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  set(FISH_IN_TREE_BUILD TRUE)
else()
  set(FISH_IN_TREE_BUILD FALSE)
endif()

# NetBSD does weird things with finding libraries,
# making the tests fail by failing to find pcre.
#
# Keep the rpath used to build.
if(CMAKE_SYSTEM_NAME STREQUAL NetBSD)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# List of other sources.
set(FISH_SRCS
    src/empty.cpp
)

# Header files are just globbed.
file(GLOB FISH_HEADERS src/*.h)

# Set up config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config_cmake.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Pull in our src directory for headers searches, but only quoted ones.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -iquote ${CMAKE_CURRENT_SOURCE_DIR}/src")

# Set up the machinery around FISH-BUILD-VERSION-FILE
# This defines the FBVF variable.
include(Version)

# Let fish pick up when we're running out of the build directory without installing
get_filename_component(REAL_CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}" REALPATH)
get_filename_component(REAL_CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}" REALPATH)
add_definitions(-DCMAKE_BINARY_DIR="${REAL_CMAKE_BINARY_DIR}")
add_definitions(-DCMAKE_SOURCE_DIR="${REAL_CMAKE_SOURCE_DIR}")

# Teach fish_version.o to rebuild when FBVF changes.
# The standard C++ include detection machinery misses this.
set_source_files_properties(src/fish_version.cpp
                            PROPERTIES OBJECT_DEPENDS
                            ${CMAKE_CURRENT_BINARY_DIR}/${FBVF})

# Enable thread-safe errno on Solaris (#5611)
add_definitions(-D_REENTRANT)

# Set up PCRE2
include(cmake/PCRE2.cmake)

# Define a function to link dependencies.
function(FISH_LINK_DEPS_AND_SIGN target)
  # Tell Cargo where our build directory is so it can find config.h.
  corrosion_set_env_vars(${target} ${VARS_FOR_CARGO})
  corrosion_link_libraries(${target} fishlib)
  codesign_on_mac(${target})
endfunction(FISH_LINK_DEPS_AND_SIGN)

# Define libfish.a.
add_library(fishlib STATIC ${FISH_SRCS})
target_sources(fishlib PRIVATE ${FISH_HEADERS})
target_link_libraries(fishlib
  ${CURSES_LIBRARY} ${CURSES_EXTRA_LIBRARY} Threads::Threads ${CMAKE_DL_LIBS}
  ${PCRE2_LIB} ${Intl_LIBRARIES} ${ATOMIC_LIBRARY})
target_include_directories(fishlib PRIVATE
  ${CURSES_INCLUDE_DIRS})

# Define fish.
fish_link_deps_and_sign(fish)

# Define fish_indent.
fish_link_deps_and_sign(fish_indent)

# Define fish_key_reader.
fish_link_deps_and_sign(fish_key_reader)

# Set up the docs.
include(cmake/Docs.cmake)

# A helper for running tests.
add_executable(fish_test_helper src/fish_test_helper.cpp)

# Set up tests.
include(cmake/Tests.cmake)

# Benchmarking support.
include(cmake/Benchmark.cmake)

# Set up install.
include(cmake/Install.cmake)

# Mac app.
include(cmake/MacApp.cmake)

# ThreadSanitizer likes to muck with signal handlers, which interferes
# with fish_test_helper printing the ignored signal mask.
# Ensure fish_test_helper does not use TSan.
# Note the environment var is CXXFLAGS, but the CMake var is CMAKE_CXX_FLAGS.
if (CMAKE_CXX_FLAGS MATCHES ".*-fsanitize=thread.*")
  target_compile_options(fish_test_helper PRIVATE "-fno-sanitize=all")
  target_link_libraries(fish_test_helper "-fno-sanitize=all")
endif()

include(FeatureSummary)
feature_summary(WHAT ALL)
