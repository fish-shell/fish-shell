CMAKE_MINIMUM_REQUIRED(VERSION 3.2)

IF(POLICY CMP0066)
  CMAKE_POLICY(SET CMP0066 OLD)
ENDIF()
IF(POLICY CMP0067)
  CMAKE_POLICY(SET CMP0067 OLD)
ENDIF()

PROJECT(fish)

# We are C++11.
SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2")
SET(DEFAULT_BUILD_TYPE "RelWithDebInfo")

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
    src/builtin_test.cpp src/builtin_ulimit.cpp src/builtin_wait.cpp
    src/color.cpp src/common.cpp src/complete.cpp src/env.cpp
    src/env_universal_common.cpp src/event.cpp src/exec.cpp src/expand.cpp
    src/fallback.cpp src/fish_version.cpp src/function.cpp src/highlight.cpp
    src/history.cpp src/input.cpp src/input_common.cpp src/intern.cpp src/io.cpp
    src/iothread.cpp src/kill.cpp src/output.cpp src/pager.cpp
    src/parse_execution.cpp src/parse_productions.cpp src/parse_tree.cpp
    src/parse_util.cpp src/parser.cpp src/parser_keywords.cpp src/path.cpp
    src/postfork.cpp src/proc.cpp src/reader.cpp src/sanity.cpp src/screen.cpp
    src/signal.cpp src/tinyexpr.cpp src/tnode.cpp src/tokenizer.cpp src/utf8.cpp src/util.cpp
    src/wcstringutil.cpp src/wgetopt.cpp src/wildcard.cpp src/wutil.cpp
    src/future_feature_flags.cpp src/redirection.cpp src/topic_monitor.cpp
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

OPTION(INTERNAL_WCWIDTH "use fallback wcwidth" ON)
# Override the emoji width detection by picking unicode9 width.
# We don't provide an option to pick unicode8 width because we'd already detect that,
# or your terminal is worse than your system wcwidth.
#
# Note that this sets the "guessed_emoji_width", so it can still be overridden via $fish_emoji_width.
OPTION(USE_UNICODE9_WIDTH "set the default emoji width to 2 (the unicode 9 value)" OFF)
IF(INTERNAL_WCWIDTH)
  IF(NOT USE_UNICODE9_WIDTH)
    # Compile and run a small program that checks an emoji for its width.
    # Note that this relies on a unicode locale, and assumes that the terminal's wcwidth
    # uses the unicode 9 widths IFF the system's does.
    #
    # So there's a bunch of assumptions, but this heuristic should still improve things.
    try_run(WCW_RUN_RESULT
      WCW_COMPILE_RESULT
      ${CMAKE_CURRENT_BINARY_DIR}/
      ${CMAKE_CURRENT_SOURCE_DIR}/test_wcwidth.cpp
      RUN_OUTPUT_VARIABLE WCW_OUTPUT)
    # If this refused to compile or run, we error out, because there's something wrong with the system. 
    if("${WCW_COMPILE_RESULT}" AND ("${WCW_RUN_RESULT}" EQUAL 0))
      # If the value is < 1 or > 2, most likely the locale is C.
      # Since that happens a bunch, fall back to unicode 8 like before.
      if("${WCW_OUTPUT}" LESS 1 OR "{WCW_OUTPUT}" GREATER 2)
        MESSAGE(WARNING "wcwidth returned unusable results. Most likely you don't have a unicode locale set. Guessing you don't have unicode9 support.")
        SET(DEFAULT_EMOJI_WIDTH 1)
      ELSE()
        # If the value is 1 or 2, we use it.
        SET(DEFAULT_EMOJI_WIDTH ${WCW_OUTPUT})
      ENDIF()
    ELSE()
      MESSAGE(FATAL_ERROR "WCWIDTH FAILED!")
    ENDIF()
  ELSE()
    SET(DEFAULT_EMOJI_WIDTH 2)
  ENDIF()

  MESSAGE("EMOJI WIDTH SET TO ${DEFAULT_EMOJI_WIDTH}")
  ADD_DEFINITIONS(-DDEFAULT_EMOJI_WIDTH=${DEFAULT_EMOJI_WIDTH})
  ADD_DEFINITIONS(-DHAVE_BROKEN_WCWIDTH=1)
ELSE()
  ADD_DEFINITIONS(-DHAVE_BROKEN_WCWIDTH=0)
ENDIF()

# Enable thread-safe errno on Solaris (#5611)
ADD_DEFINITIONS(-D_REENTRANT)

# Set up PCRE2
INCLUDE(cmake/PCRE2.cmake)

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
  ${PCRE2_LIB} ${Intl_LIBRARIES})

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
