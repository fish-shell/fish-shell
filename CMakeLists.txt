CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
PROJECT(fish-shell CXX)

# We are C++11.
SET(CMAKE_CXX_STANDARD 11)

# Work around cmake/ninja bug that fails on install with ninja
# See https://cmake.org/Bug/print_bug_page.php?bug_id=13934
if (NINJA)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH 1)
endif(NINJA)

# Disable exception handling.
ADD_COMPILE_OPTIONS(-fno-exceptions)

# Hide the CMake Rules directories in Xcode projects.
SOURCE_GROUP("CMake Rules" REGULAR_EXPRESSION "^$")

# Put source and header files at top level under targets.
SOURCE_GROUP("Source Files" REGULAR_EXPRESSION "^$")
SOURCE_GROUP("Header Files" REGULAR_EXPRESSION "^$")
SOURCE_GROUP("Builtins" REGULAR_EXPRESSION "builtin_.*")

# Support folders.
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# All objects that the system needs to build fish, except fish.cpp
SET(FISH_SRCS
    src/autoload.cpp src/builtin.cpp src/builtin_bg.cpp src/builtin_bind.cpp
    src/builtin_block.cpp src/builtin_builtin.cpp src/builtin_cd.cpp
    src/builtin_command.cpp src/builtin_commandline.cpp
    src/builtin_complete.cpp src/builtin_contains.cpp src/builtin_disown.cpp
    src/builtin_echo.cpp src/builtin_emit.cpp src/builtin_exit.cpp
    src/builtin_fg.cpp src/builtin_function.cpp src/builtin_functions.cpp
    src/builtin_argparse.cpp src/builtin_history.cpp src/builtin_jobs.cpp
    src/builtin_math.cpp src/builtin_printf.cpp src/builtin_pwd.cpp
    src/builtin_random.cpp src/builtin_read.cpp src/builtin_realpath.cpp
    src/builtin_return.cpp src/builtin_set.cpp src/builtin_set_color.cpp
    src/builtin_source.cpp src/builtin_status.cpp src/builtin_string.cpp
    src/builtin_test.cpp src/builtin_ulimit.cpp src/builtin_wait.cpp
    src/color.cpp src/common.cpp src/complete.cpp src/env.cpp
    src/env_universal_common.cpp src/event.cpp src/exec.cpp src/expand.cpp
    src/fallback.cpp src/fish_version.cpp src/function.cpp src/highlight.cpp
    src/history.cpp src/input.cpp src/input_common.cpp src/intern.cpp src/io.cpp
    src/iothread.cpp src/kill.cpp src/output.cpp src/pager.cpp
    src/parse_execution.cpp src/parse_productions.cpp src/parse_tree.cpp
    src/parse_util.cpp src/parser.cpp src/parser_keywords.cpp src/path.cpp
    src/postfork.cpp src/proc.cpp src/reader.cpp src/sanity.cpp src/screen.cpp
    src/signal.cpp src/tokenizer.cpp src/utf8.cpp src/util.cpp
    src/wcstringutil.cpp src/wgetopt.cpp src/wildcard.cpp src/wutil.cpp
)

# Header files are just globbed.
FILE(GLOB FISH_HEADERS src/*.h)

# git_version_gen.sh creates FISH-BUILD-VERSION-FILE if it does not exist, and updates it
# only if it is out-of-date.
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/build_tools/cmake_git_version_gen.sh"
    OUTPUT_VARIABLE FISH_BUILD_VERSION)

message(STATUS "FISH_BUILD_VERSION: ${FISH_BUILD_VERSION}")
ADD_DEFINITIONS(-DFISH_BUILD_VERSION="${FISH_BUILD_VERSION}")

# Set up fish-build-version.h
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/fish_build_version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/fish_build_version.h")
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

# Set up config.h
INCLUDE(cmake/ConfigureChecks.cmake)
INCLUDE(cmake/gettext.cmake)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config_cmake.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Set up standard directories.
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-DCMAKE
                -D_UNICODE=1
                -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
                -DPREFIX=L"${CMAKE_INSTALL_PREFIX}"
                -DDATADIR=L"${CMAKE_INSTALL_FULL_DATADIR}"
                -DSYSCONFDIR=L"${CMAKE_INSTALL_FULL_SYSCONFDIR}"
                -DBINDIR=L"${CMAKE_INSTALL_FULL_BINDIR}"
                -DDOCDIR=L"${CMAKE_INSTALL_FULL_DOCDIR}")

# Set up PCRE2
INCLUDE(cmake/PCRE2.cmake)

# Set up muparser.
INCLUDE(muparser-2.2.5/CMakeLists.txt)

# Set up the docs.
INCLUDE(cmake/Docs.cmake)

# Define a function to link dependencies.
FUNCTION(FISH_LINK_DEPS target)
  TARGET_LINK_LIBRARIES(${target} fishlib)
ENDFUNCTION(FISH_LINK_DEPS)

# Define libfish.a.
ADD_LIBRARY(fishlib STATIC ${FISH_SRCS})
TARGET_SOURCES(fishlib PRIVATE ${FISH_HEADERS})
TARGET_LINK_LIBRARIES(fishlib
  ${CURSES_LIBRARY} ${CURSES_EXTRA_LIBRARY} Threads::Threads ${CMAKE_DL_LIBS}
  ${PCRE2_LIB} muparser)

# Define fish.
ADD_EXECUTABLE(fish src/fish.cpp)
FISH_LINK_DEPS(fish)

# Define fish_indent.
ADD_EXECUTABLE(fish_indent
               src/fish_indent.cpp src/print_help.cpp)
FISH_LINK_DEPS(fish_indent)

# Define fish_key_reader.
ADD_EXECUTABLE(fish_key_reader
               src/fish_key_reader.cpp src/print_help.cpp)
FISH_LINK_DEPS(fish_key_reader)

# Set up tests.
INCLUDE(cmake/Tests.cmake)

# Set up install.
INCLUDE(cmake/Install.cmake)

INCLUDE(FeatureSummary)
FEATURE_SUMMARY(WHAT ALL)
