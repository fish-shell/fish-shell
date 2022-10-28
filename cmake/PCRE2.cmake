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
  "Use PCRE2 from the system, instead of fetching and building it")

if(FISH_USE_SYSTEM_PCRE2)
  set(PCRE2_LIB "${SYS_PCRE2_LIB}")
  set(PCRE2_INCLUDE_DIR "${SYS_PCRE2_INCLUDE_DIR}")
  message(STATUS "Using system PCRE2 library ${PCRE2_INCLUDE_DIR}")
else()
  include(FetchContent RESULT_VARIABLE HAVE_FetchContent)
  if (${HAVE_FetchContent} STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Please install PCRE2 headers, or CMake >= 3.11 so I can download PCRE")
  endif()
  set(CMAKE_TLS_VERIFY true)
  set(PCRE2_REPO "https://github.com/PCRE2Project/pcre2.git")

  message(STATUS "Fetching and configuring PCRE2 from ${PCRE2_REPO}")
  Set(FETCHCONTENT_QUIET FALSE)
  FetchContent_Declare(
    pcre2
    GIT_REPOSITORY ${PCRE2_REPO}
    GIT_TAG "pcre2-10.36"
    GIT_PROGRESS TRUE
  )
  # Don't try FetchContent_MakeAvailable, there's no way to add EXCLUDE_FROM_ALL
  # so we end up installing all of PCRE2 including its headers, man pages, etc.
  FetchContent_GetProperties(pcre2)
  if (NOT pcre2_POPULATED)
    # If GIT_WORK_TREE is set (by user or by git itself with e.g. git rebase), it
    # will override the git directory which CMake tries to apply in FetchContent_Populate,
    # resulting in a failed checkout.
    # Ensure it is not set.
    unset(ENV{GIT_WORK_TREE})
    unset(ENV{GIT_DIR})
    FetchContent_Populate(pcre2)
    add_subdirectory(${pcre2_SOURCE_DIR} ${pcre2_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()

  set(PCRE2_INCLUDE_DIR ${pcre2_BINARY_DIR})
  set(PCRE2_LIB pcre2-${PCRE2_WIDTH})

  # Disable -Wunused-macros inside PCRE2, as it is noisy.
  get_target_property(PCRE2_COMPILE_OPTIONS ${PCRE2_LIB} COMPILE_OPTIONS)
  list(REMOVE_ITEM PCRE2_COMPILE_OPTIONS "-Wunused-macros")
  set_property(TARGET ${PCRE2_LIB} PROPERTY COMPILE_OPTIONS ${PCRE2_COMPILE_OPTIONS})

endif(FISH_USE_SYSTEM_PCRE2)
include_directories(${PCRE2_INCLUDE_DIR})
