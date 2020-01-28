CMAKE_MINIMUM_REQUIRED(VERSION 3.2)

IF(POLICY CMP0066)
  CMAKE_POLICY(SET CMP0066 OLD)
ENDIF()
IF(POLICY CMP0067)
  CMAKE_POLICY(SET CMP0067 NEW)
ENDIF()

PROJECT(fish)

# We are C++11.
SET(CMAKE_CXX_STANDARD 11)
SET(DEFAULT_BUILD_TYPE "RelWithDebInfo")

# Use the default flags (#6296) but remove -DNDEBUG so that asserts remain enabled.
STRING(REPLACE "-DNDEBUG" ""
  CMAKE_CXX_FLAGS_RELWITHDEBINFO
  "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

STRING(REPLACE "-DNDEBUG" ""
  CMAKE_CXX_FLAGS_RELEASE
  "${CMAKE_CXX_FLAGS_RELEASE}")

IF(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    MESSAGE(STATUS "Setting build type to default '${DEFAULT_BUILD_TYPE}'")
    SET(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
ENDIF()

# Force colored warnings in Ninja's output, if the compiler has -fdiagnostics-color support.
# Rationale in https://github.com/ninja-build/ninja/issues/814
if (CMAKE_GENERATOR STREQUAL "Ninja" AND
    ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9) OR
     (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5) OR
     (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)))
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()
# Enable a whole bunch of warnings, but turn off:
# - implicit fallthrough because that does not recognize some cases where it's desired (and I *really* want this one!)
# - comment because we use a bunch of those, and they're not really all that harmful.
# - address, because that occurs for our mkostemp check (weak-linking requires us to compare `&mkostemp == nullptr`).
# - strict-aliasing, because on old GCCs (*Travis*) those are triggered by maybe.h, so you get it every time it is included.
# - redundant-move, because we have one that is required on old libc
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra \
-Wno-implicit-fallthrough \
-Wno-comment \
-Wno-address \
-Wno-strict-aliasing \
-Wno-redundant-move \
")

# Disable exception handling.
ADD_COMPILE_OPTIONS(-fno-exceptions)

# Prefer the gold linker because it doesn't emit useless warnings about sys_nerr and _sys_errlist.
if (UNIX AND NOT APPLE)
  EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version
                  ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
  if ("${LD_VERSION}" MATCHES "GNU gold")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
  endif()
endif()

# Hide the CMake Rules directories in Xcode projects.
SOURCE_GROUP("CMake Rules" REGULAR_EXPRESSION "^$")

# Put source and header files at top level under targets.
SOURCE_GROUP("Source Files" REGULAR_EXPRESSION "^$")
SOURCE_GROUP("Header Files" REGULAR_EXPRESSION "^$")
SOURCE_GROUP("Builtins" REGULAR_EXPRESSION "builtin_.*")

# Support folders.
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

# Work around issue where archive-built libs go in the wrong place.
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

IF(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  SET(FISH_IN_TREE_BUILD TRUE)
ELSE()
  SET(FISH_IN_TREE_BUILD FALSE)
ENDIF()

# NetBSD does weird things with finding libraries,
# making the tests fail by failing to find pcre.
#
# Keep the rpath used to build.
IF(CMAKE_SYSTEM_NAME STREQUAL NetBSD)
  SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
ENDIF()

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
    src/builtin_test.cpp src/builtin_ulimit.cpp src/builtin_wait.cpp src/builtin_eval.cpp
    src/color.cpp src/common.cpp src/complete.cpp src/env.cpp src/env_dispatch.cpp
    src/env_universal_common.cpp src/event.cpp src/exec.cpp src/expand.cpp
    src/fallback.cpp src/fish_version.cpp src/function.cpp src/highlight.cpp
    src/history.cpp src/history_file.cpp src/input.cpp src/input_common.cpp src/intern.cpp
    src/io.cpp src/iothread.cpp src/kill.cpp src/output.cpp src/pager.cpp
    src/parse_execution.cpp src/parse_productions.cpp src/parse_tree.cpp
    src/parse_util.cpp src/parser.cpp src/parser_keywords.cpp src/path.cpp
    src/postfork.cpp src/proc.cpp src/reader.cpp src/sanity.cpp src/screen.cpp
    src/signal.cpp src/tinyexpr.cpp src/tnode.cpp src/tokenizer.cpp src/utf8.cpp src/util.cpp
    src/wcstringutil.cpp src/wgetopt.cpp src/wildcard.cpp src/wutil.cpp
    src/future_feature_flags.cpp src/redirection.cpp src/topic_monitor.cpp
    src/flog.cpp src/trace.cpp src/timer.cpp src/null_terminated_array.cpp
    src/operation_context.cpp
)

# Header files are just globbed.
FILE(GLOB FISH_HEADERS src/*.h)

# Set up config.h
INCLUDE(cmake/ConfigureChecks.cmake)
INCLUDE(cmake/gettext.cmake)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config_cmake.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Set up standard directories.
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-D_UNICODE=1
                -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
                -DPREFIX=L"${CMAKE_INSTALL_PREFIX}"
                -DDATADIR=L"${CMAKE_INSTALL_FULL_DATADIR}"
                -DSYSCONFDIR=L"${CMAKE_INSTALL_FULL_SYSCONFDIR}"
                -DBINDIR=L"${CMAKE_INSTALL_FULL_BINDIR}"
                -DDOCDIR=L"${CMAKE_INSTALL_FULL_DOCDIR}")

# Set up the machinery around FISH-BUILD-VERSION-FILE
# This defines the FBVF variable.
INCLUDE(Version)

# Let fish pick up when we're running out of the build directory without installing
GET_FILENAME_COMPONENT(REAL_CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}" REALPATH)
GET_FILENAME_COMPONENT(REAL_CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}" REALPATH)
ADD_DEFINITIONS(-DCMAKE_BINARY_DIR="${REAL_CMAKE_BINARY_DIR}")
ADD_DEFINITIONS(-DCMAKE_SOURCE_DIR="${REAL_CMAKE_SOURCE_DIR}")

# Teach fish_version.o to rebuild when FBVF changes.
# The standard C++ include detection machinery misses this.
SET_SOURCE_FILES_PROPERTIES(src/fish_version.cpp
                            PROPERTIES OBJECT_DEPENDS
                            ${CMAKE_CURRENT_BINARY_DIR}/${FBVF})

# Enable thread-safe errno on Solaris (#5611)
ADD_DEFINITIONS(-D_REENTRANT)

# Set up PCRE2
INCLUDE(cmake/PCRE2.cmake)

# Define a function to link dependencies.
FUNCTION(FISH_LINK_DEPS target)
  TARGET_LINK_LIBRARIES(${target} fishlib)
ENDFUNCTION(FISH_LINK_DEPS)

# Define libfish.a.
ADD_LIBRARY(fishlib STATIC ${FISH_SRCS})
TARGET_SOURCES(fishlib PRIVATE ${FISH_HEADERS})
TARGET_LINK_LIBRARIES(fishlib
  ${CURSES_LIBRARY} ${CURSES_EXTRA_LIBRARY} Threads::Threads ${CMAKE_DL_LIBS}
  ${PCRE2_LIB} ${Intl_LIBRARIES} ${ATOMIC_LIBRARY})

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

# Set up the docs.
INCLUDE(cmake/Docs.cmake)

# A helper for running tests.
ADD_EXECUTABLE(fish_test_helper src/fish_test_helper.cpp)

# Set up tests.
INCLUDE(cmake/Tests.cmake)

# Benchmarking support.
INCLUDE(cmake/Benchmark.cmake)

# Set up install.
INCLUDE(cmake/Install.cmake)

# Mac app.
INCLUDE(cmake/MacApp.cmake)

# Lint targets
# This could be implemented as target properties, but the script has the useful feature of only
# checking the currently-staged commands
# The generator expressions below rebuild the command line for the fishlib targets
# CMake does not support the "iquote" flag - https://gitlab.kitware.com/cmake/cmake/issues/15491
SET(LINT_ARGS "-D$<JOIN:$<TARGET_PROPERTY:fishlib,COMPILE_DEFINITIONS>, -D>" "-I$<JOIN:$<TARGET_PROPERTY:fishlib,INCLUDE_DIRECTORIES>, -I>")
ADD_CUSTOM_TARGET(lint
    COMMAND build_tools/lint.fish -- ${LINT_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
ADD_CUSTOM_TARGET(lint-all
    COMMAND build_tools/lint.fish --all -- ${LINT_ARGS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

INCLUDE(FeatureSummary)
FEATURE_SUMMARY(WHAT ALL)
