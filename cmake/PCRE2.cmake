# PCRE2 needs some settings.
set(PCRE2_WIDTH ${WCHAR_T_BITS})
set(PCRE2_BUILD_PCRE2_8 OFF CACHE BOOL "Build 8bit PCRE2 library")
set(PCRE2_BUILD_PCRE2_${PCRE2_WIDTH} ON CACHE BOOL "Build ${PCRE2_WIDTH}bit PCRE2 library")
set(PCRE2_SHOW_REPORT OFF CACHE BOOL "Show the final configuration report")
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(PCRE2_BUILD_PCRE2GREP OFF CACHE BOOL "Build pcre2grep")

set(PCRE2_MIN_VERSION 10.21)

# Look for a system-installed PCRE2.
find_library(SYS_PCRE2_LIB pcre2-${PCRE2_WIDTH})
find_path(SYS_PCRE2_INCLUDE_DIR pcre2.h)

# We can either use the system-installed PCRE or our bundled version.
# This is controlled by the cache variable FISH_USE_SYSTEM_PCRE2.
# Here we compute the default value for that variable.
if ((APPLE) AND (MAC_CODESIGN_ID))
  # On Mac, a codesigned fish will refuse to load a non-codesigned PCRE2
  # (e.g. from Homebrew) so default to bundled PCRE2.
  set(USE_SYS_PCRE2_DEFAULT OFF)
elseif((NOT SYS_PCRE2_LIB) OR (NOT SYS_PCRE2_INCLUDE_DIR))
  # We did not find system PCRE2, so default to bundled.
  set(USE_SYS_PCRE2_DEFAULT OFF)
else()
  # Default to using the system PCRE2, which was found.
  set(USE_SYS_PCRE2_DEFAULT ON)
endif()

set(FISH_USE_SYSTEM_PCRE2 ${USE_SYS_PCRE2_DEFAULT} CACHE BOOL
  "Use PCRE2 from the system, instead of bundled with fish")

if(FISH_USE_SYSTEM_PCRE2)
  set(PCRE2_LIB "${SYS_PCRE2_LIB}")
  set(PCRE2_INCLUDE_DIR "${SYS_PCRE2_INCLUDE_DIR}")
  message(STATUS "Using system PCRE2 library ${PCRE2_INCLUDE_DIR}")
else()
  message(STATUS "Using bundled PCRE2 library")
  add_subdirectory(pcre2 EXCLUDE_FROM_ALL)
  set(PCRE2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/pcre2)
  set(PCRE2_LIB pcre2-${PCRE2_WIDTH})
endif(FISH_USE_SYSTEM_PCRE2)
include_directories(${PCRE2_INCLUDE_DIR})
