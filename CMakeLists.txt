PROJECT(fish-shell)
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

# We are C++11.
SET(CMAKE_CXX_STANDARD 11)

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
    src/builtin_test.cpp src/builtin_ulimit.cpp src/color.cpp src/common.cpp
    src/complete.cpp src/env.cpp src/env_universal_common.cpp src/event.cpp
    src/exec.cpp src/expand.cpp src/fallback.cpp src/fish_version.cpp
    src/function.cpp src/highlight.cpp src/history.cpp src/input.cpp
    src/input_common.cpp src/intern.cpp src/io.cpp src/iothread.cpp
    src/kill.cpp src/output.cpp src/pager.cpp src/parse_execution.cpp
    src/parse_productions.cpp src/parse_tree.cpp src/parse_util.cpp
    src/parser.cpp src/parser_keywords.cpp src/path.cpp src/postfork.cpp
    src/proc.cpp src/reader.cpp src/sanity.cpp src/screen.cpp src/signal.cpp
    src/tokenizer.cpp src/utf8.cpp src/util.cpp src/wcstringutil.cpp
    src/wgetopt.cpp src/wildcard.cpp src/wutil.cpp
)

# Set up FISH-BUILD-VERSION-FILE
# Note this happens when CMake runs, not when the generated build system runs
EXECUTE_PROCESS(COMMAND build_tools/git_version_gen.sh
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_QUIET)

# Set up config.h
INCLUDE(CMakeFiles/ConfigureChecks.cmake)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config_cmake.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Set up standard directories.
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-D_UNICODE=1
                -DFISH_BUILD_VERSION="${FISH_BUILD_VERSION}"
                -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
                -DPREFIX=L"${CMAKE_INSTALL_PREFIX}"
                -DDATADIR=L"${CMAKE_INSTALL_FULL_DATADIR}"
                -DSYSCONFDIR=L"${CMAKE_INSTALL_FULL_SYSCONFDIR}"
                -DBINDIR=L"${CMAKE_INSTALL_FULL_BINDIR}"
                -DDOCDIR=L"${CMAKE_INSTALL_FULL_DOCDIR}")

# Set up PCRE2
INCLUDE(CMakeFiles/PCRE2.cmake)

# Set up muparser.
INCLUDE(CMakeFiles/MuParser.cmake)

# Set up the docs.
INCLUDE(CMakeFiles/Docs.cmake)

# Define a function to link dependencies.
FUNCTION(FISH_LINK_DEPS target)
  TARGET_LINK_LIBRARIES(${target} ${CURSES_LIBRARIES} Threads::Threads)
  TARGET_LINK_LIBRARIES(${target} ${PCRE2_LIB} muparser)
ENDFUNCTION(FISH_LINK_DEPS)

# Define fish.
ADD_EXECUTABLE(fish src/fish.cpp ${FISH_SRCS})
FISH_LINK_DEPS(fish)

# Define fish_indent.
ADD_EXECUTABLE(fish_indent
               src/fish_indent.cpp src/print_help.cpp ${FISH_SRCS})
FISH_LINK_DEPS(fish_indent)

# Define fish_key_reader.
ADD_EXECUTABLE(fish_key_reader
               src/fish_key_reader.cpp src/print_help.cpp ${FISH_SRCS})
FISH_LINK_DEPS(fish_key_reader)

# Set up tests.
INCLUDE(CMakeFiles/Tests.cmake)
