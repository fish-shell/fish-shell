# CMakeLists.txt
#
#
# This file enables PCRE2 to be built with the CMake configuration and build
# tool. Download CMake in source or binary form from http://www.cmake.org/
# Converted to support PCRE2 from the original PCRE file, August 2014.
#
# Original listfile by Christian Ehrlicher <Ch.Ehrlicher@gmx.de>
# Refined and expanded by Daniel Richard G. <skunk@iSKUNK.ORG>
# 2007-09-14 mod by Sheri so 7.4 supported configuration options can be entered
# 2007-09-19 Adjusted by PH to retain previous default settings
# 2007-12-26 (a) On UNIX, use names libpcre instead of just pcre
#            (b) Ensure pcretest and pcregrep link with the local library,
#                not a previously-installed one.
#            (c) Add PCRE_SUPPORT_LIBREADLINE, PCRE_SUPPORT_LIBZ, and
#                PCRE_SUPPORT_LIBBZ2.
# 2008-01-20 Brought up to date to include several new features by Christian
#            Ehrlicher.
# 2008-01-22 Sheri added options for backward compatibility of library names
#            when building with minGW:
#            if "ON", NON_STANDARD_LIB_PREFIX causes shared libraries to
#            be built without "lib" as prefix. (The libraries will be named
#            pcre.dll, pcreposix.dll and pcrecpp.dll).
#            if "ON", NON_STANDARD_LIB_SUFFIX causes shared libraries to
#            be built with suffix of "-0.dll". (The libraries will be named
#            libpcre-0.dll, libpcreposix-0.dll and libpcrecpp-0.dll - same names
#            built by default with Configure and Make.
# 2008-01-23 PH removed the automatic build of pcredemo.
# 2008-04-22 PH modified READLINE support so it finds NCURSES when needed.
# 2008-07-03 PH updated for revised UCP property support (change of files)
# 2009-03-23 PH applied Steven Van Ingelgem's patch to change the name
#            CMAKE_BINARY_DIR to PROJECT_BINARY_DIR so that it works when PCRE
#            is included within another project.
# 2009-03-23 PH applied a modified version of Steven Van Ingelgem's patches to
#            add options to stop the building of pcregrep and the tests, and
#            to disable the final configuration report.
# 2009-04-11 PH applied Christian Ehrlicher's patch to show compiler flags that
#            are set by specifying a release type.
# 2010-01-02 PH added test for stdint.h
# 2010-03-02 PH added test for inttypes.h
# 2011-08-01 PH added PCREGREP_BUFSIZE
# 2011-08-22 PH added PCRE_SUPPORT_JIT
# 2011-09-06 PH modified WIN32 ADD_TEST line as suggested by Sergey Cherepanov
# 2011-09-06 PH added PCRE_SUPPORT_PCREGREP_JIT
# 2011-10-04 Sheri added support for including coff data in windows shared libraries
#            compiled with MINGW if pcre.rc and/or pcreposix.rc are placed in
#            the source dir by the user prior to building
# 2011-10-04 Sheri changed various add_test's to use exes' location built instead
#            of DEBUG location only (likely only matters in MSVC)
# 2011-10-04 Sheri added scripts to provide needed variables to RunTest and
#            RunGrepTest (used for UNIX and Msys)
# 2011-10-04 Sheri added scripts to provide needed variables and to execute
#            RunTest.bat in Win32 (for effortless testing with "make test")
# 2011-10-04 Sheri Increased minimum required cmake version
# 2012-01-06 PH removed pcre_info.c and added pcre_string_utils.c
# 2012-01-10 Zoltan Herczeg added libpcre16 support
# 2012-01-13 Stephen Kelly added out of source build support
# 2012-01-17 PH applied Stephen Kelly's patch to parse the version data out
#            of the configure.ac file
# 2012-02-26 PH added support for libedit
# 2012-09-06 PH added support for PCRE_EBCDIC_NL25
# 2012-09-08 ChPe added PCRE32 support
# 2012-10-23 PH added support for VALGRIND and GCOV
# 2012-12-08 PH added patch from Daniel Richard G to quash some MSVC warnings
# 2013-07-01 PH realized that the "support" for GCOV was a total nonsense and
#            so it has been removed.
# 2013-10-08 PH got rid of the "source" command, which is a bash-ism (use ".")
# 2013-11-05 PH added support for PARENS_NEST_LIMIT
# 2014-08-29 PH converted the file for PCRE2 (which has no C++).
# 2015-04-24 PH added support for PCRE2_DEBUG
# 2015-07-16 PH updated for new pcre2_find_bracket source module
# 2015-08-24 PH correct C_FLAGS setting (patch from Roy Ivy III)
# 2015-10=16 PH added support for never-backslash-C
# 2016-03-01 PH applied Chris Wilson's patch for MSVC static
# 2016-06-24 PH applied Chris Wilson's second patch, putting the first under
#            a new option instead of being unconditional.

PROJECT(PCRE2 C)

# Increased minimum to 2.8.0 to support newer add_test features. Set policy
# CMP0026 to avoid warnings for the use of LOCATION in GET_TARGET_PROPERTY.

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.0)
CMAKE_POLICY(SET CMP0026 OLD)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake) # for FindReadline.cmake

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${PROJECT_SOURCE_DIR}/src")

# external packages
FIND_PACKAGE( BZip2 )
FIND_PACKAGE( ZLIB )
FIND_PACKAGE( Readline )
FIND_PACKAGE( Editline )

# Configuration checks

INCLUDE(CheckIncludeFile)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckTypeSize)

CHECK_INCLUDE_FILE(dirent.h     HAVE_DIRENT_H)
CHECK_INCLUDE_FILE(stdint.h     HAVE_STDINT_H)
CHECK_INCLUDE_FILE(inttypes.h   HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(sys/stat.h   HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/types.h  HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(unistd.h     HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(windows.h    HAVE_WINDOWS_H)

CHECK_FUNCTION_EXISTS(bcopy     HAVE_BCOPY)
CHECK_FUNCTION_EXISTS(memmove   HAVE_MEMMOVE)
CHECK_FUNCTION_EXISTS(strerror  HAVE_STRERROR)

# User-configurable options
#
# Note: CMakeSetup displays these in alphabetical order, regardless of
# the order we use here.

SET(BUILD_SHARED_LIBS OFF CACHE BOOL
    "Build shared libraries instead of static ones.")

OPTION(PCRE2_BUILD_PCRE2_8 "Build 8 bit PCRE2 library" ON)

OPTION(PCRE2_BUILD_PCRE2_16 "Build 16 bit PCRE2 library" OFF)

OPTION(PCRE2_BUILD_PCRE2_32 "Build 32 bit PCRE2 library" OFF)

OPTION(PCRE2_DEBUG "Include debugging code" OFF)

SET(PCRE2_EBCDIC OFF CACHE BOOL
    "Use EBCDIC coding instead of ASCII. (This is rarely used outside of mainframe systems.)")

SET(PCRE2_EBCDIC_NL25 OFF CACHE BOOL
    "Use 0x25 as EBCDIC NL character instead of 0x15; implies EBCDIC.")

SET(PCRE2_LINK_SIZE "2" CACHE STRING
    "Internal link size (2, 3 or 4 allowed). See LINK_SIZE in config.h.in for details.")

SET(PCRE2_PARENS_NEST_LIMIT "250" CACHE STRING
    "Default nested parentheses limit. See PARENS_NEST_LIMIT in config.h.in for details.")

SET(PCRE2_MATCH_LIMIT "10000000" CACHE STRING
    "Default limit on internal looping. See MATCH_LIMIT in config.h.in for details.")

SET(PCRE2_MATCH_LIMIT_RECURSION "MATCH_LIMIT" CACHE STRING
    "Default limit on internal recursion. See MATCH_LIMIT_RECURSION in config.h.in for details.")

SET(PCRE2GREP_BUFSIZE "20480" CACHE STRING
    "Buffer size parameter for pcre2grep. See PCRE2GREP_BUFSIZE in config.h.in for details.")

SET(PCRE2_NEWLINE "LF" CACHE STRING
    "What to recognize as a newline (one of CR, LF, CRLF, ANY, ANYCRLF).")

SET(PCRE2_HEAP_MATCH_RECURSE OFF CACHE BOOL
    "If ON, then don't use stack recursion when matching. See HEAP_MATCH_RECURSE in config.h.in for details.")

SET(PCRE2_SUPPORT_JIT OFF CACHE BOOL
    "Enable support for Just-in-time compiling.")

SET(PCRE2_SUPPORT_PCRE2GREP_JIT ON CACHE BOOL
    "Enable use of Just-in-time compiling in pcre2grep.")

SET(PCRE2_SUPPORT_PCRE2GREP_CALLOUT ON CACHE BOOL
    "Enable callout string support in pcre2grep.")

SET(PCRE2_SUPPORT_UNICODE ON CACHE BOOL
    "Enable support for Unicode and UTF-8/UTF-16/UTF-32 encoding.")

SET(PCRE2_SUPPORT_BSR_ANYCRLF OFF CACHE BOOL
    "ON=Backslash-R matches only LF CR and CRLF, OFF=Backslash-R matches all Unicode Linebreaks")

SET(PCRE2_NEVER_BACKSLASH_C OFF CACHE BOOL
    "If ON, backslash-C (upper case C) is locked out.")

SET(PCRE2_SUPPORT_VALGRIND OFF CACHE BOOL
    "Enable Valgrind support.")

OPTION(PCRE2_SHOW_REPORT     "Show the final configuration report" ON)
OPTION(PCRE2_BUILD_PCRE2GREP "Build pcre2grep" ON)
OPTION(PCRE2_BUILD_TESTS     "Build the tests" ON)

IF (MINGW)
  OPTION(NON_STANDARD_LIB_PREFIX
         "ON=Shared libraries built in mingw will be named pcre2.dll, etc., instead of libpcre2.dll, etc."
         OFF)

  OPTION(NON_STANDARD_LIB_SUFFIX
         "ON=Shared libraries built in mingw will be named libpcre2-0.dll, etc., instead of libpcre2.dll, etc."
         OFF)
ENDIF(MINGW)

IF(MSVC)
  OPTION(PCRE_STATIC_RUNTIME OFF CACHE BOOL
	"ON=Compile against the static runtime (/MT)."
	OFF)
  OPTION(INSTALL_MSVC_PDB
         "ON=Install .pdb files built by MSVC, if generated"
         OFF)
ENDIF(MSVC)

# bzip2 lib
IF(BZIP2_FOUND)
  OPTION (PCRE2_SUPPORT_LIBBZ2 "Enable support for linking pcre2grep with libbz2." ON)
ENDIF(BZIP2_FOUND)
IF(PCRE2_SUPPORT_LIBBZ2)
  INCLUDE_DIRECTORIES(${BZIP2_INCLUDE_DIR})
ENDIF(PCRE2_SUPPORT_LIBBZ2)

# zlib
IF(ZLIB_FOUND)
  OPTION (PCRE2_SUPPORT_LIBZ "Enable support for linking pcre2grep with libz." ON)
ENDIF(ZLIB_FOUND)
IF(PCRE2_SUPPORT_LIBZ)
  INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
ENDIF(PCRE2_SUPPORT_LIBZ)

# editline lib
IF(EDITLINE_FOUND)
  OPTION (PCRE2_SUPPORT_LIBEDIT  "Enable support for linking pcre2test with libedit." OFF)
ENDIF(EDITLINE_FOUND)
IF(PCRE2_SUPPORT_LIBEDIT)
  INCLUDE_DIRECTORIES(${EDITLINE_INCLUDE_DIR})
ENDIF(PCRE2_SUPPORT_LIBEDIT)

# readline lib
IF(READLINE_FOUND)
  OPTION (PCRE2_SUPPORT_LIBREADLINE  "Enable support for linking pcre2test with libreadline." ON)
ENDIF(READLINE_FOUND)
IF(PCRE2_SUPPORT_LIBREADLINE)
  INCLUDE_DIRECTORIES(${READLINE_INCLUDE_DIR})
ENDIF(PCRE2_SUPPORT_LIBREADLINE)

# Prepare build configuration

IF(NOT BUILD_SHARED_LIBS)
        SET(PCRE2_STATIC 1)
ENDIF(NOT BUILD_SHARED_LIBS)

IF(NOT PCRE2_BUILD_PCRE2_8 AND NOT PCRE2_BUILD_PCRE2_16 AND NOT PCRE2_BUILD_PCRE2_32)
        MESSAGE(FATAL_ERROR "At least one of PCRE2_BUILD_PCRE2_8, PCRE2_BUILD_PCRE2_16 or PCRE2_BUILD_PCRE2_32 must be enabled")
ENDIF(NOT PCRE2_BUILD_PCRE2_8 AND NOT PCRE2_BUILD_PCRE2_16 AND NOT PCRE2_BUILD_PCRE2_32)

IF(PCRE2_BUILD_PCRE2_8)
        SET(SUPPORT_PCRE2_8 1)
ENDIF(PCRE2_BUILD_PCRE2_8)

IF(PCRE2_BUILD_PCRE2_16)
        SET(SUPPORT_PCRE2_16 1)
ENDIF(PCRE2_BUILD_PCRE2_16)

IF(PCRE2_BUILD_PCRE2_32)
        SET(SUPPORT_PCRE2_32 1)
ENDIF(PCRE2_BUILD_PCRE2_32)

IF(PCRE2_BUILD_PCRE2GREP AND NOT PCRE2_BUILD_PCRE2_8)
        MESSAGE(STATUS "** PCRE2_BUILD_PCRE2_8 must be enabled for the pcre2grep program")
        SET(PCRE2_BUILD_PCRE2GREP OFF)
ENDIF(PCRE2_BUILD_PCRE2GREP AND NOT PCRE2_BUILD_PCRE2_8)

IF(PCRE2_SUPPORT_LIBREADLINE AND PCRE2_SUPPORT_LIBEDIT)
        MESSAGE(FATAL_ERROR "Only one of libreadline or libeditline can be specified")
ENDIF(PCRE2_SUPPORT_LIBREADLINE AND PCRE2_SUPPORT_LIBEDIT)

IF(PCRE2_SUPPORT_BSR_ANYCRLF)
        SET(BSR_ANYCRLF 1)
ENDIF(PCRE2_SUPPORT_BSR_ANYCRLF)

IF(PCRE2_NEVER_BACKSLASH_C)
        SET(NEVER_BACKSLASH_C 1)
ENDIF(PCRE2_NEVER_BACKSLASH_C)

IF(PCRE2_SUPPORT_UNICODE)
        SET(SUPPORT_UNICODE 1)
ENDIF(PCRE2_SUPPORT_UNICODE)

IF(PCRE2_SUPPORT_JIT)
        SET(SUPPORT_JIT 1)
ENDIF(PCRE2_SUPPORT_JIT)

IF(PCRE2_SUPPORT_PCRE2GREP_JIT)
        SET(SUPPORT_PCRE2GREP_JIT 1)
ENDIF(PCRE2_SUPPORT_PCRE2GREP_JIT)

IF(PCRE2_SUPPORT_PCRE2GREP_CALLOUT)
        SET(SUPPORT_PCRE2GREP_CALLOUT 1)
ENDIF(PCRE2_SUPPORT_PCRE2GREP_CALLOUT)

IF(PCRE2_SUPPORT_VALGRIND)
        SET(SUPPORT_VALGRIND 1)
ENDIF(PCRE2_SUPPORT_VALGRIND)

# This next one used to reference ${READLINE_LIBRARY})
# but I was advised to add the NCURSES test as well, along with
# some modifications to cmake/FindReadline.cmake which should
# make it possible to override the default if necessary. PH

IF(PCRE2_SUPPORT_LIBREADLINE)
        SET(SUPPORT_LIBREADLINE 1)
        SET(PCRE2TEST_LIBS ${READLINE_LIBRARY} ${NCURSES_LIBRARY})
ENDIF(PCRE2_SUPPORT_LIBREADLINE)

# libedit is a plug-compatible alternative to libreadline

IF(PCRE2_SUPPORT_LIBEDIT)
        SET(SUPPORT_LIBEDIT 1)
        SET(PCRE2TEST_LIBS ${EDITLINE_LIBRARY} ${NCURSES_LIBRARY})
ENDIF(PCRE2_SUPPORT_LIBEDIT)

IF(PCRE2_SUPPORT_LIBZ)
        SET(SUPPORT_LIBZ 1)
        SET(PCRE2GREP_LIBS ${PCRE2GREP_LIBS} ${ZLIB_LIBRARIES})
ENDIF(PCRE2_SUPPORT_LIBZ)

IF(PCRE2_SUPPORT_LIBBZ2)
        SET(SUPPORT_LIBBZ2 1)
        SET(PCRE2GREP_LIBS ${PCRE2GREP_LIBS} ${BZIP2_LIBRARIES})
ENDIF(PCRE2_SUPPORT_LIBBZ2)

SET(NEWLINE_DEFAULT "")

IF(PCRE2_NEWLINE STREQUAL "CR")
        SET(NEWLINE_DEFAULT "1")
ENDIF(PCRE2_NEWLINE STREQUAL "CR")
IF(PCRE2_NEWLINE STREQUAL "LF")
        SET(NEWLINE_DEFAULT "2")
ENDIF(PCRE2_NEWLINE STREQUAL "LF")
IF(PCRE2_NEWLINE STREQUAL "CRLF")
        SET(NEWLINE_DEFAULT "3")
ENDIF(PCRE2_NEWLINE STREQUAL "CRLF")
IF(PCRE2_NEWLINE STREQUAL "ANY")
        SET(NEWLINE_DEFAULT "4")
ENDIF(PCRE2_NEWLINE STREQUAL "ANY")
IF(PCRE2_NEWLINE STREQUAL "ANYCRLF")
        SET(NEWLINE_DEFAULT "5")
ENDIF(PCRE2_NEWLINE STREQUAL "ANYCRLF")

IF(NEWLINE_DEFAULT STREQUAL "")
        MESSAGE(FATAL_ERROR "The PCRE2_NEWLINE variable must be set to one of the following values: \"LF\", \"CR\", \"CRLF\", \"ANY\", \"ANYCRLF\".")
ENDIF(NEWLINE_DEFAULT STREQUAL "")

IF(PCRE2_EBCDIC)
        SET(EBCDIC 1)
ENDIF(PCRE2_EBCDIC)

IF(PCRE2_EBCDIC_NL25)
        SET(EBCDIC 1)
        SET(EBCDIC_NL25 1)
ENDIF(PCRE2_EBCDIC_NL25)

IF(PCRE2_HEAP_MATCH_RECURSE)
        SET(HEAP_MATCH_RECURSE 1)
ENDIF(PCRE2_HEAP_MATCH_RECURSE)

# Output files

CONFIGURE_FILE(config-cmake.h.in
               ${PROJECT_BINARY_DIR}/config.h
               @ONLY)

# Parse version numbers and date out of configure.ac

file(STRINGS ${PROJECT_SOURCE_DIR}/configure.ac
  configure_lines
  LIMIT_COUNT 50 # Read only the first 50 lines of the file
)

set(SEARCHED_VARIABLES "pcre2_major" "pcre2_minor" "pcre2_prerelease" "pcre2_date")
foreach(configure_line ${configure_lines})
    foreach(_substitution_variable ${SEARCHED_VARIABLES})
        string(TOUPPER ${_substitution_variable} _substitution_variable_upper)
        if (NOT ${_substitution_variable_upper})
            string(REGEX MATCH "m4_define\\(${_substitution_variable}, \\[(.*)\\]" MACTHED_STRING ${configure_line})
            if (CMAKE_MATCH_1)
                set(${_substitution_variable_upper} ${CMAKE_MATCH_1})
            endif()
        endif()
    endforeach()
endforeach()

CONFIGURE_FILE(src/pcre2.h.in
               ${PROJECT_BINARY_DIR}/pcre2.h
               @ONLY)

# What about pcre2-config and libpcre2.pc?

# Character table generation

OPTION(PCRE2_REBUILD_CHARTABLES "Rebuild char tables" OFF)
IF(PCRE2_REBUILD_CHARTABLES)
  ADD_EXECUTABLE(dftables src/dftables.c)
  ADD_CUSTOM_COMMAND(
    COMMENT "Generating character tables (pcre2_chartables.c) for current locale"
    DEPENDS dftables
    COMMAND dftables
    ARGS        ${PROJECT_BINARY_DIR}/pcre2_chartables.c
    OUTPUT      ${PROJECT_BINARY_DIR}/pcre2_chartables.c
  )
ELSE(PCRE2_REBUILD_CHARTABLES)
  CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/src/pcre2_chartables.c.dist
                    ${PROJECT_BINARY_DIR}/pcre2_chartables.c
                    COPYONLY)
ENDIF(PCRE2_REBUILD_CHARTABLES)

# Source code

SET(PCRE2_HEADERS ${PROJECT_BINARY_DIR}/pcre2.h)

SET(PCRE2_SOURCES
  src/pcre2_auto_possess.c
  ${PROJECT_BINARY_DIR}/pcre2_chartables.c
  src/pcre2_compile.c
  src/pcre2_config.c
  src/pcre2_context.c
  src/pcre2_dfa_match.c
  src/pcre2_error.c
  src/pcre2_find_bracket.c
  src/pcre2_jit_compile.c
  src/pcre2_maketables.c
  src/pcre2_match.c
  src/pcre2_match_data.c
  src/pcre2_newline.c
  src/pcre2_ord2utf.c
  src/pcre2_pattern_info.c
  src/pcre2_serialize.c
  src/pcre2_string_utils.c
  src/pcre2_study.c
  src/pcre2_substitute.c
  src/pcre2_substring.c
  src/pcre2_tables.c
  src/pcre2_ucd.c
  src/pcre2_valid_utf.c
  src/pcre2_xclass.c
)

SET(PCRE2POSIX_HEADERS src/pcre2posix.h)
SET(PCRE2POSIX_SOURCES src/pcre2posix.c)

IF(MINGW AND NOT PCRE2_STATIC)
IF (EXISTS ${PROJECT_SOURCE_DIR}/pcre2.rc)
ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_SOURCE_DIR}/pcre2.o
PRE-LINK
COMMAND windres ARGS pcre2.rc pcre2.o
WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
COMMENT Using pcre2 coff info in mingw build)
SET(PCRE2_SOURCES
  ${PCRE2_SOURCES} ${PROJECT_SOURCE_DIR}/pcre2.o
)
ENDIF(EXISTS ${PROJECT_SOURCE_DIR}/pcre2.rc)
IF (EXISTS ${PROJECT_SOURCE_DIR}/pcre2posix.rc)
ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_SOURCE_DIR}/pcre2posix.o
PRE-LINK
COMMAND windres ARGS pcre2posix.rc pcre2posix.o
WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
COMMENT Using pcre2posix coff info in mingw build)
SET(PCRE2POSIX_SOURCES
  ${PCRE2POSIX_SOURCES} ${PROJECT_SOURCE_DIR}/pcre2posix.o
)
ENDIF(EXISTS ${PROJECT_SOURCE_DIR}/pcre2posix.rc)
ENDIF(MINGW AND NOT PCRE2_STATIC)

IF(MSVC AND NOT PCRE2_STATIC)
IF (EXISTS ${PROJECT_SOURCE_DIR}/pcre2.rc)
SET(PCRE2_SOURCES
  ${PCRE2_SOURCES} pcre2.rc)
ENDIF(EXISTS ${PROJECT_SOURCE_DIR}/pcre2.rc)
IF (EXISTS ${PROJECT_SOURCE_DIR}/pcre2posix.rc)
SET(PCRE2POSIX_SOURCES
  ${PCRE2POSIX_SOURCES} pcre2posix.rc)
ENDIF (EXISTS ${PROJECT_SOURCE_DIR}/pcre2posix.rc)
ENDIF(MSVC AND NOT PCRE2_STATIC)

# Fix static compilation with MSVC: https://bugs.exim.org/show_bug.cgi?id=1681
# This code was taken from the CMake wiki, not from WebM.

IF(MSVC AND PCRE2_STATIC_RUNTIME)
  MESSAGE(STATUS "** MSVC and PCRE2_STATIC_RUNTIME: modifying compiler flags to use static runtime library")
  foreach(flag_var
          CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
          CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
  endforeach()
ENDIF(MSVC AND PCRE2_STATIC_RUNTIME)

# Build setup

ADD_DEFINITIONS(-DHAVE_CONFIG_H)

IF(MSVC)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
ENDIF(MSVC)

SET(CMAKE_INCLUDE_CURRENT_DIR 1)
# needed to make sure to not link debug libs
# against release libs and vice versa
IF(WIN32)
  SET(CMAKE_DEBUG_POSTFIX "d")
ENDIF(WIN32)

SET(targets)

# 8-bit library

IF(PCRE2_BUILD_PCRE2_8)
ADD_LIBRARY(pcre2-8 ${PCRE2_HEADERS} ${PCRE2_SOURCES} ${PROJECT_BINARY_DIR}/config.h)
SET_PROPERTY(TARGET pcre2-8
  PROPERTY COMPILE_DEFINITIONS PCRE2_CODE_UNIT_WIDTH=8)
SET(targets ${targets} pcre2-8)
ADD_LIBRARY(pcre2posix ${PCRE2POSIX_HEADERS} ${PCRE2POSIX_SOURCES})
SET_PROPERTY(TARGET pcre2posix
  PROPERTY COMPILE_DEFINITIONS PCRE2_CODE_UNIT_WIDTH=8)
SET(targets ${targets} pcre2posix)
TARGET_LINK_LIBRARIES(pcre2posix pcre2-8)

IF(MINGW AND NOT PCRE2_STATIC)
  IF(NON_STANDARD_LIB_PREFIX)
    SET_TARGET_PROPERTIES(pcre2-8 pcre2posix PROPERTIES PREFIX "")
  ENDIF(NON_STANDARD_LIB_PREFIX)
  IF(NON_STANDARD_LIB_SUFFIX)
    SET_TARGET_PROPERTIES(pcre2-8 pcre2posix PROPERTIES SUFFIX "-0.dll")
  ENDIF(NON_STANDARD_LIB_SUFFIX)
ENDIF(MINGW AND NOT PCRE2_STATIC)
ENDIF(PCRE2_BUILD_PCRE2_8)

# 16-bit library

IF(PCRE2_BUILD_PCRE2_16)
ADD_LIBRARY(pcre2-16 ${PCRE2_HEADERS} ${PCRE2_SOURCES} ${PROJECT_BINARY_DIR}/config.h)
SET_PROPERTY(TARGET pcre2-16
  PROPERTY COMPILE_DEFINITIONS PCRE2_CODE_UNIT_WIDTH=16)
SET(targets ${targets} pcre2-16)

IF(MINGW AND NOT PCRE2_STATIC)
  IF(NON_STANDARD_LIB_PREFIX)
    SET_TARGET_PROPERTIES(pcre2-16 PROPERTIES PREFIX "")
  ENDIF(NON_STANDARD_LIB_PREFIX)
  IF(NON_STANDARD_LIB_SUFFIX)
    SET_TARGET_PROPERTIES(pcre2-16 PROPERTIES SUFFIX "-0.dll")
  ENDIF(NON_STANDARD_LIB_SUFFIX)
ENDIF(MINGW AND NOT PCRE2_STATIC)
ENDIF(PCRE2_BUILD_PCRE2_16)

# 32-bit library

IF(PCRE2_BUILD_PCRE2_32)
ADD_LIBRARY(pcre2-32 ${PCRE2_HEADERS} ${PCRE2_SOURCES} ${PROJECT_BINARY_DIR}/config.h)
SET_PROPERTY(TARGET pcre2-32
  PROPERTY COMPILE_DEFINITIONS PCRE2_CODE_UNIT_WIDTH=32)
SET(targets ${targets} pcre2-32)

IF(MINGW AND NOT PCRE2_STATIC)
  IF(NON_STANDARD_LIB_PREFIX)
    SET_TARGET_PROPERTIES(pcre2-32 PROPERTIES PREFIX "")
  ENDIF(NON_STANDARD_LIB_PREFIX)
  IF(NON_STANDARD_LIB_SUFFIX)
    SET_TARGET_PROPERTIES(pcre2-32 PROPERTIES SUFFIX "-0.dll")
  ENDIF(NON_STANDARD_LIB_SUFFIX)
ENDIF(MINGW AND NOT PCRE2_STATIC)
ENDIF(PCRE2_BUILD_PCRE2_32)

# Executables

IF(PCRE2_BUILD_PCRE2GREP)
  ADD_EXECUTABLE(pcre2grep src/pcre2grep.c)
  SET_PROPERTY(TARGET pcre2grep
    PROPERTY COMPILE_DEFINITIONS PCRE2_CODE_UNIT_WIDTH=8)
  SET(targets ${targets} pcre2grep)
  TARGET_LINK_LIBRARIES(pcre2grep pcre2posix ${PCRE2GREP_LIBS})
ENDIF(PCRE2_BUILD_PCRE2GREP)

# Testing

IF(PCRE2_BUILD_TESTS)
  ENABLE_TESTING()

  SET(PCRE2TEST_SOURCES src/pcre2test.c)

  ADD_EXECUTABLE(pcre2test ${PCRE2TEST_SOURCES})
  SET(targets ${targets} pcre2test)
  IF(PCRE2_BUILD_PCRE2_8)
    LIST(APPEND PCRE2TEST_LIBS pcre2posix pcre2-8)
  ENDIF(PCRE2_BUILD_PCRE2_8)
  IF(PCRE2_BUILD_PCRE2_16)
    LIST(APPEND PCRE2TEST_LIBS pcre2-16)
  ENDIF(PCRE2_BUILD_PCRE2_16)
  IF(PCRE2_BUILD_PCRE2_32)
    LIST(APPEND PCRE2TEST_LIBS pcre2-32)
  ENDIF(PCRE2_BUILD_PCRE2_32)
  TARGET_LINK_LIBRARIES(pcre2test ${PCRE2TEST_LIBS})

  IF(PCRE2_SUPPORT_JIT)
    ADD_EXECUTABLE(pcre2_jit_test src/pcre2_jit_test.c)
    SET(targets ${targets} pcre2_jit_test)
    SET(PCRE2_JIT_TEST_LIBS )
    IF(PCRE2_BUILD_PCRE2_8)
      LIST(APPEND PCRE2_JIT_TEST_LIBS pcre2-8)
    ENDIF(PCRE2_BUILD_PCRE2_8)
    IF(PCRE2_BUILD_PCRE2_16)
      LIST(APPEND PCRE2_JIT_TEST_LIBS pcre2-16)
    ENDIF(PCRE2_BUILD_PCRE2_16)
    IF(PCRE2_BUILD_PCRE2_32)
      LIST(APPEND PCRE2_JIT_TEST_LIBS pcre2-32)
    ENDIF(PCRE2_BUILD_PCRE2_32)
    TARGET_LINK_LIBRARIES(pcre2_jit_test ${PCRE2_JIT_TEST_LIBS})
  ENDIF(PCRE2_SUPPORT_JIT)

  # exes in Debug location tested by the RunTest shell script
  # via "make test"

  IF(PCRE2_BUILD_PCRE2GREP)
    GET_TARGET_PROPERTY(PCRE2GREP_EXE pcre2grep DEBUG_LOCATION)
  ENDIF(PCRE2_BUILD_PCRE2GREP)

  GET_TARGET_PROPERTY(PCRE2TEST_EXE pcre2test DEBUG_LOCATION)

# =================================================
  # Write out a CTest configuration file
  #
  FILE(WRITE ${PROJECT_BINARY_DIR}/CTestCustom.ctest
  "# This is a generated file.
MESSAGE(\"When testing is complete, review test output in the
\\\"${PROJECT_BINARY_DIR}/Testing/Temporary\\\" folder.\")
MESSAGE(\" \")
")

  FILE(WRITE ${PROJECT_BINARY_DIR}/pcre2_test.sh
  "#! /bin/sh
# This is a generated file.
. ${PROJECT_SOURCE_DIR}/RunTest
if test \"$?\" != \"0\"; then exit 1; fi
# End
")

  IF(UNIX)
    ADD_TEST(pcre2_test sh ${PROJECT_BINARY_DIR}/pcre2_test.sh)
  ENDIF(UNIX)

  IF(PCRE2_BUILD_PCRE2GREP)
    FILE(WRITE ${PROJECT_BINARY_DIR}/pcre2_grep_test.sh
    "#! /bin/sh
# This is a generated file.
. ${PROJECT_SOURCE_DIR}/RunGrepTest
if test \"$?\" != \"0\"; then exit 1; fi
# End
")

    IF(UNIX)
      ADD_TEST(pcre2_grep_test sh ${PROJECT_BINARY_DIR}/pcre2_grep_test.sh)
    ENDIF(UNIX)
  ENDIF(PCRE2_BUILD_PCRE2GREP)

  IF(WIN32)
    # Provide environment for executing the bat file version of RunTest
    FILE(TO_NATIVE_PATH ${PROJECT_SOURCE_DIR} winsrc)
    FILE(TO_NATIVE_PATH ${PROJECT_BINARY_DIR} winbin)
    FILE(TO_NATIVE_PATH ${PCRE2TEST_EXE} winexe)

    FILE(WRITE ${PROJECT_BINARY_DIR}/pcre2_test.bat
    "\@REM This is a generated file.
\@echo off
setlocal
SET srcdir=\"${winsrc}\"
SET pcre2test=\"${winexe}\"
if not [%CMAKE_CONFIG_TYPE%]==[] SET pcre2test=\"${winbin}\\%CMAKE_CONFIG_TYPE%\\pcre2test.exe\"
call %srcdir%\\RunTest.Bat
if errorlevel 1 exit /b 1
echo RunTest.bat tests successfully completed
")

  ADD_TEST(NAME pcre2_test_bat
  COMMAND pcre2_test.bat)
  SET_TESTS_PROPERTIES(pcre2_test_bat PROPERTIES
  PASS_REGULAR_EXPRESSION "RunTest\\.bat tests successfully completed")

    IF("$ENV{OSTYPE}" STREQUAL "msys")
      # Both the sh and bat file versions of RunTest are run if make test is used
      # in msys
      ADD_TEST(pcre2_test_sh    sh.exe ${PROJECT_BINARY_DIR}/pcre2_test.sh)
      IF(PCRE2_BUILD_PCRE2GREP)
        ADD_TEST(pcre2_grep_test  sh.exe ${PROJECT_BINARY_DIR}/pcre2_grep_test.sh)
      ENDIF(PCRE2_BUILD_PCRE2GREP)
    ENDIF("$ENV{OSTYPE}" STREQUAL "msys")
  ENDIF(WIN32)

  # Changed to accommodate testing whichever location was just built

  IF(PCRE2_SUPPORT_JIT)
    ADD_TEST(pcre2_jit_test pcre2_jit_test)
  ENDIF(PCRE2_SUPPORT_JIT)

ENDIF(PCRE2_BUILD_TESTS)

# Installation

SET(CMAKE_INSTALL_ALWAYS 1)

INSTALL(TARGETS ${targets}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

INSTALL(FILES ${PCRE2_HEADERS} ${PCRE2POSIX_HEADERS} DESTINATION include)

FILE(GLOB html ${PROJECT_SOURCE_DIR}/doc/html/*.html)
FILE(GLOB man1 ${PROJECT_SOURCE_DIR}/doc/*.1)
FILE(GLOB man3 ${PROJECT_SOURCE_DIR}/doc/*.3)

FOREACH(man ${man3})
        GET_FILENAME_COMPONENT(man_tmp ${man} NAME)
        SET(man3_new ${man3} ${man})
ENDFOREACH(man ${man3})
SET(man3 ${man3_new})

INSTALL(FILES ${man1} DESTINATION man/man1)
INSTALL(FILES ${man3} DESTINATION man/man3)
INSTALL(FILES ${html} DESTINATION share/doc/pcre2/html)

IF(MSVC AND INSTALL_MSVC_PDB)
    INSTALL(FILES ${PROJECT_BINARY_DIR}/pcre2.pdb
                  ${PROJECT_BINARY_DIR}/pcre2posix.pdb
            DESTINATION bin
            CONFIGURATIONS RelWithDebInfo)
    INSTALL(FILES ${PROJECT_BINARY_DIR}/pcre2d.pdb
                  ${PROJECT_BINARY_DIR}/pcre2posixd.pdb
            DESTINATION bin
            CONFIGURATIONS Debug)
ENDIF(MSVC AND INSTALL_MSVC_PDB)

# Help, only for nice output
IF(BUILD_SHARED_LIBS)
  SET(BUILD_STATIC_LIBS OFF)
ELSE(BUILD_SHARED_LIBS)
  SET(BUILD_STATIC_LIBS ON)
ENDIF(BUILD_SHARED_LIBS)

IF(PCRE2_SHOW_REPORT)
  STRING(TOUPPER "${CMAKE_BUILD_TYPE}" buildtype)
  IF (CMAKE_C_FLAGS)
    SET(cfsp " ")
  ENDIF(CMAKE_C_FLAGS)
  MESSAGE(STATUS "")
  MESSAGE(STATUS "")
  MESSAGE(STATUS "PCRE2 configuration summary:")
  MESSAGE(STATUS "")
  MESSAGE(STATUS "  Install prefix .................. : ${CMAKE_INSTALL_PREFIX}")
  MESSAGE(STATUS "  C compiler ...................... : ${CMAKE_C_COMPILER}")
  MESSAGE(STATUS "  C compiler flags ................ : ${CMAKE_C_FLAGS}${cfsp}${CMAKE_C_FLAGS_${buildtype}}")
  MESSAGE(STATUS "")
  MESSAGE(STATUS "  Build 8 bit PCRE2 library ....... : ${PCRE2_BUILD_PCRE2_8}")
  MESSAGE(STATUS "  Build 16 bit PCRE2 library ...... : ${PCRE2_BUILD_PCRE2_16}")
  MESSAGE(STATUS "  Build 32 bit PCRE2 library ...... : ${PCRE2_BUILD_PCRE2_32}")
  MESSAGE(STATUS "  Enable JIT compiling support .... : ${PCRE2_SUPPORT_JIT}")
  MESSAGE(STATUS "  Enable Unicode support .......... : ${PCRE2_SUPPORT_UNICODE}")
  MESSAGE(STATUS "  Newline char/sequence ........... : ${PCRE2_NEWLINE}")
  MESSAGE(STATUS "  \\R matches only ANYCRLF ......... : ${PCRE2_SUPPORT_BSR_ANYCRLF}")
  MESSAGE(STATUS "  \\C is disabled .................. : ${PCRE2_NEVER_BACKSLASH_C}")
  MESSAGE(STATUS "  EBCDIC coding ................... : ${PCRE2_EBCDIC}")
  MESSAGE(STATUS "  EBCDIC coding with NL=0x25 ...... : ${PCRE2_EBCDIC_NL25}")
  MESSAGE(STATUS "  Rebuild char tables ............. : ${PCRE2_REBUILD_CHARTABLES}")
  MESSAGE(STATUS "  Use heap recursion .............. : ${PCRE2_HEAP_MATCH_RECURSE}")
  MESSAGE(STATUS "  Internal link size .............. : ${PCRE2_LINK_SIZE}")
  MESSAGE(STATUS "  Parentheses nest limit .......... : ${PCRE2_PARENS_NEST_LIMIT}")
  MESSAGE(STATUS "  Match limit ..................... : ${PCRE2_MATCH_LIMIT}")
  MESSAGE(STATUS "  Match limit recursion ........... : ${PCRE2_MATCH_LIMIT_RECURSION}")
  MESSAGE(STATUS "  Build shared libs ............... : ${BUILD_SHARED_LIBS}")
  MESSAGE(STATUS "  Build static libs ............... : ${BUILD_STATIC_LIBS}")
  MESSAGE(STATUS "  Build pcre2grep ................. : ${PCRE2_BUILD_PCRE2GREP}")
  MESSAGE(STATUS "  Enable JIT in pcre2grep ......... : ${PCRE2_SUPPORT_PCRE2GREP_JIT}")
  MESSAGE(STATUS "  Enable callouts in pcre2grep .... : ${PCRE2_SUPPORT_PCRE2GREP_CALLOUT}")
  MESSAGE(STATUS "  Buffer size for pcre2grep ....... : ${PCRE2GREP_BUFSIZE}")
  MESSAGE(STATUS "  Build tests (implies pcre2test  . : ${PCRE2_BUILD_TESTS}")
  MESSAGE(STATUS "               and pcre2grep)")
  IF(ZLIB_FOUND)
    MESSAGE(STATUS "  Link pcre2grep with libz ........ : ${PCRE2_SUPPORT_LIBZ}")
  ELSE(ZLIB_FOUND)
    MESSAGE(STATUS "  Link pcre2grep with libz ........ : Library not found" )
  ENDIF(ZLIB_FOUND)
  IF(BZIP2_FOUND)
    MESSAGE(STATUS "  Link pcre2grep with libbz2 ...... : ${PCRE2_SUPPORT_LIBBZ2}")
  ELSE(BZIP2_FOUND)
    MESSAGE(STATUS "  Link pcre2grep with libbz2 ...... : Library not found" )
  ENDIF(BZIP2_FOUND)
  IF(EDITLINE_FOUND)
    MESSAGE(STATUS "  Link pcre2test with libeditline . : ${PCRE2_SUPPORT_LIBEDIT}")
  ELSE(EDITLINE_FOUND)
    MESSAGE(STATUS "  Link pcre2test with libeditline . : Library not found" )
  ENDIF(EDITLINE_FOUND)
  IF(READLINE_FOUND)
    MESSAGE(STATUS "  Link pcre2test with libreadline . : ${PCRE2_SUPPORT_LIBREADLINE}")
  ELSE(READLINE_FOUND)
    MESSAGE(STATUS "  Link pcre2test with libreadline . : Library not found" )
  ENDIF(READLINE_FOUND)
  MESSAGE(STATUS "  Support Valgrind .................: ${PCRE2_SUPPORT_VALGRIND}")

  IF(MINGW AND NOT PCRE2_STATIC)
    MESSAGE(STATUS "  Non-standard dll names (prefix) . : ${NON_STANDARD_LIB_PREFIX}")
    MESSAGE(STATUS "  Non-standard dll names (suffix) . : ${NON_STANDARD_LIB_SUFFIX}")
  ENDIF(MINGW AND NOT PCRE2_STATIC)

  IF(MSVC)
    MESSAGE(STATUS "  Install MSVC .pdb files ..........: ${INSTALL_MSVC_PDB}")
  ENDIF(MSVC)

  MESSAGE(STATUS "")
ENDIF(PCRE2_SHOW_REPORT)

# end CMakeLists.txt
