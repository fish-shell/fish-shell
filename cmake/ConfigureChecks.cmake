# The following defines affect the environment configuration tests are run in:
# CMAKE_REQUIRED_DEFINITIONS, CMAKE_REQUIRED_FLAGS, CMAKE_REQUIRED_LIBRARIES,
# and CMAKE_REQUIRED_INCLUDES
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE=1)
include(CheckCXXCompilerFlag)
include(CMakePushCheckState)

if(APPLE)
    check_cxx_compiler_flag("-Werror=unguarded-availability" REQUIRES_UNGUARDED_AVAILABILITY)
    if(REQUIRES_UNGUARDED_AVAILABILITY)
        list(APPEND CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} "-Werror=unguarded-availability")
    endif()
endif()

# An unrecognized flag is usually a warning and not an error, which CMake apparently does
# not pick up on. Combine it with -Werror to determine if it's actually supported.
# This is not bulletproof; old versions of GCC only emit a warning about unrecognized warning
# options when there are other warnings to emit :rolleyes:
# See https://github.com/fish-shell/fish-shell/commit/fe2da0a9#commitcomment-47431659

# GCC supports -Wno-redundant-move from GCC9 onwards
check_cxx_compiler_flag("-Werror=no-redundant-move" HAS_NO_REDUNDANT_MOVE)
if (HAS_NO_REDUNDANT_MOVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-redundant-move")
endif()
# Clang once supported -Wno-redundant-move but replaced it with a Wredundant-move option instead
# (and it is functionally different from its older version of GCC's Wno-redundant-move).
check_cxx_compiler_flag("-Werror=redundant-move" HAS_REDUNDANT_MOVE)
if (HAS_REDUNDANT_MOVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wredundant-move")
endif()

# Defeat bogus warnings about missing field initializers for `var{}` initialization.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    cmake_push_check_state()
    list(APPEND CMAKE_REQUIRED_FLAGS "-W")
    check_cxx_source_compiles("
    struct sr_t { int x; };
    int main(void) {
        sr_t sr{};
        return sr.x;
    }"
    EMPTY_VALUE_INIT_ACCEPTED
    FAIL_REGEX "-Wmissing-field-initializers"
    )
    if (NOT EMPTY_VALUE_INIT_ACCEPTED)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
    endif()
    cmake_pop_check_state()
  endif()

# Disable static destructors if we can.
check_cxx_compiler_flag("-fno-c++-static-destructors" DISABLE_STATIC_DESTRUCTORS)
if (DISABLE_STATIC_DESTRUCTORS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-c++-static-destructors")
endif()


# Try using CMake's own logic to locate curses/ncurses
find_package(Curses)
if(NOT ${CURSES_FOUND})
    # CMake has trouble finding platform-specific system libraries
    # installed to multiarch paths (e.g. /usr/lib/x86_64-linux-gnu)
    # if not symlinked or passed in as a manual define.
    message("Falling back to pkg-config for (n)curses detection")
    include(FindPkgConfig)
    pkg_search_module(CURSES REQUIRED ncurses curses)
    set(CURSES_CURSES_LIBRARY ${CURSES_LIBRARIES})
    set(CURSES_LIBRARY ${CURSES_LIBRARIES})
endif()
# Set up extra include directories for CheckIncludeFile
list(APPEND CMAKE_REQUIRED_INCLUDES ${CURSES_INCLUDE_DIRS})

# Fix undefined reference to tparm on RHEL 6 and potentially others
# If curses is found via CMake, it also links against tinfo if it exists. But if we use our
# fallback pkg-config logic above, we need to do this manually.
find_library(CURSES_TINFO tinfo)
if (CURSES_TINFO)
    set(CURSES_LIBRARY ${CURSES_LIBRARY} ${CURSES_TINFO})
else()
    # on NetBSD, libtinfo has a longer name (libterminfo)
    find_library(CURSES_TINFO terminfo)
    if (CURSES_TINFO)
        set(CURSES_LIBRARY ${CURSES_LIBRARY} ${CURSES_TINFO})
    endif()
endif()

# Get threads.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Detect WSL. Does not match against native Windows/WIN32.
if (CMAKE_HOST_SYSTEM_VERSION MATCHES ".*-Microsoft")
  set(WSL 1)
endif()

# Set up the config.h file.
set(PACKAGE_NAME "fish")
set(PACKAGE_TARNAME "fish")
include(CheckCXXSymbolExists)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckStructHasMember)
include(CheckCXXSourceCompiles)
include(CheckTypeSize)
check_cxx_symbol_exists(backtrace_symbols execinfo.h HAVE_BACKTRACE_SYMBOLS)

# workaround for lousy mtime precision on a Linux kernel
if (CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
    check_cxx_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)
    check_cxx_symbol_exists(futimens sys/stat.h HAVE_FUTIMENS)
    if ((HAVE_CLOCK_GETTIME) AND (HAVE_FUTIMENS))
        set(UVAR_FILE_SET_MTIME_HACK 1)
    endif()
endif()

check_struct_has_member("struct dirent" d_type dirent.h HAVE_STRUCT_DIRENT_D_TYPE LANGUAGE CXX)
check_cxx_symbol_exists(dirfd "sys/types.h;dirent.h" HAVE_DIRFD)
check_include_file_cxx(execinfo.h HAVE_EXECINFO_H)
check_cxx_symbol_exists(getpwent pwd.h HAVE_GETPWENT)
check_cxx_symbol_exists(getrusage sys/resource.h HAVE_GETRUSAGE)
check_cxx_symbol_exists(gettext libintl.h HAVE_GETTEXT)
check_cxx_symbol_exists(killpg "sys/types.h;signal.h" HAVE_KILLPG)
# mkostemp is in stdlib in glibc and FreeBSD, but unistd on macOS
check_cxx_symbol_exists(mkostemp "stdlib.h;unistd.h" HAVE_MKOSTEMP)
set(HAVE_CURSES_H ${CURSES_HAVE_CURSES_H})
set(HAVE_NCURSES_CURSES_H ${CURSES_HAVE_NCURSES_CURSES_H})
set(HAVE_NCURSES_H ${CURSES_HAVE_NCURSES_H})
if(HAVE_CURSES_H)
    check_include_files("curses.h;term.h" HAVE_TERM_H)
endif()
if(NOT HAVE_TERM_H)
    check_include_file_cxx("ncurses/term.h" HAVE_NCURSES_TERM_H)
endif()
check_include_file_cxx(siginfo.h HAVE_SIGINFO_H)
check_include_file_cxx(spawn.h HAVE_SPAWN_H)
check_struct_has_member("struct stat" st_ctime_nsec "sys/stat.h" HAVE_STRUCT_STAT_ST_CTIME_NSEC
    LANGUAGE CXX)
check_struct_has_member("struct stat" st_mtimespec.tv_nsec "sys/stat.h"
    HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC LANGUAGE CXX)
check_struct_has_member("struct stat" st_mtim.tv_nsec "sys/stat.h" HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
    LANGUAGE CXX)
check_include_file_cxx(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file_cxx(sys/select.h HAVE_SYS_SELECT_H)

# glibc 2.30 deprecated <sys/sysctl.h> because that's what glibc does.
# Checking for that here rather than hardcoding a check on the glibc
# version in the C++ sources at point of use makes more sense.
SET(OLD_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
check_include_files("sys/types.h;sys/sysctl.h" HAVE_SYS_SYSCTL_H)
SET(CMAKE_C_FLAGS "${OLD_CMAKE_C_FLAGS}")

check_cxx_symbol_exists(wcscasecmp wchar.h HAVE_WCSCASECMP)
check_cxx_symbol_exists(wcsncasecmp wchar.h HAVE_WCSNCASECMP)

# These are for compatibility with Solaris 10, which places the following
# in the std namespace.
if(NOT HAVE_WCSNCASECMP)
    check_cxx_symbol_exists(std::wcscasecmp wchar.h HAVE_STD__WCSCASECMP)
    check_cxx_symbol_exists(std::wcsncasecmp wchar.h HAVE_STD__WCSNCASECMP)
endif()

check_include_files("xlocale.h" HAVE_XLOCALE_H)

cmake_push_check_state()
check_struct_has_member("struct winsize" ws_row "termios.h;sys/ioctl.h" _HAVE_WINSIZE)
check_cxx_symbol_exists("TIOCGWINSZ" "termios.h;sys/ioctl.h" HAVE_TIOCGWINSZ)
if(_HAVE_WINSIZE EQUAL 1 AND HAVE_TIOCGWINSZ EQUAL 1)
  set(HAVE_WINSIZE 1)
endif()
cmake_pop_check_state()

check_type_size("wchar_t[8]" WCHAR_T_BITS LANGUAGE CXX)

set(TPARM_INCLUDES)
if(HAVE_NCURSES_H)
  set(TPARM_INCLUDES "${TPARM_INCLUDES}#include <ncurses.h>\n")
elseif(HAVE_NCURSES_CURSES_H)
  set(TPARM_INCLUDES "${TPARM_INCLUDES}#include <ncurses/curses.h>\n")
else()
  set(TPARM_INCLUDES "${TPARM_INCLUDES}#include <curses.h>\n")
endif()

if(HAVE_TERM_H)
  set(TPARM_INCLUDES "${TPARM_INCLUDES}#include <term.h>\n")
elseif(HAVE_NCURSES_TERM_H)
  set(TPARM_INCLUDES "${TPARM_INCLUDES}#include <ncurses/term.h>\n")
endif()

cmake_push_check_state()
list(APPEND CMAKE_REQUIRED_LIBRARIES ${CURSES_LIBRARY})
# Solaris and X/Open-conforming systems have a fixed-args tparm
check_cxx_source_compiles("
#define TPARM_VARARGS
${TPARM_INCLUDES}

int main () {
  tparm( \"\" );
}
"
  TPARM_TAKES_VARARGS
)


# Check if tputs needs a function reading an int or char.
# The only curses I can find that needs a char is OpenIndiana.
check_cxx_source_compiles("
#include <curses.h>
#include <term.h>

static int writer(int b) {
    return b;
}

int main() {
    return tputs(\"foo\", 5, writer);
}"
TPUTS_USES_INT_ARG
)

if(TPARM_TAKES_VARARGS)
  set(TPARM_VARARGS 1)
else()
  set(TPARM_SOLARIS_KLUDGE 1)
endif()
cmake_pop_check_state()

# Work around the fact that cmake does not propagate the language standard flag into
# the CHECK_CXX_SOURCE_COMPILES function. See CMake issue #16456.
# Ensure we do this after the FIND_PACKAGE calls which use C, and will error on a C++
# standards flag.
# Also see https://github.com/fish-shell/fish-shell/issues/5865
if(NOT POLICY CMP0067)
  list(APPEND CMAKE_REQUIRED_FLAGS "${CMAKE_CXX${CMAKE_CXX_STANDARD}_EXTENSION_COMPILE_OPTION}")
endif()

check_cxx_source_compiles("
#include <memory>

int main () {
  std::unique_ptr<int> foo = std::make_unique<int>();
}
"
  HAVE_STD__MAKE_UNIQUE
)

# Detect support for thread_local.
check_cxx_source_compiles("
int main () {
  static thread_local int x = 3;
  (void)x;
}
"
  HAVE_CX11_THREAD_LOCAL
)

check_cxx_source_compiles("
#include <atomic>
#include <cstdint>
std::atomic<uint8_t> n8 (0);
std::atomic<uint64_t> n64 (0);
int main() {
uint8_t i = n8.load(std::memory_order_relaxed);
uint64_t j = n64.load(std::memory_order_relaxed);
return std::atomic_is_lock_free(&n8)
     & std::atomic_is_lock_free(&n64);
}"
LIBATOMIC_NOT_NEEDED)
IF (NOT LIBATOMIC_NOT_NEEDED)
    set(ATOMIC_LIBRARY "atomic")
endif()

check_cxx_source_compiles("
#include <sys/wait.h>

int main() {
    static_assert(WEXITSTATUS(0x007f) == 0x7f, \"This is our message we need to add because C++ is terrible\");
    return 0;
}
"
HAVE_WAITSTATUS_SIGNAL_RET)

IF (APPLE)
    # Check if mbrtowc implementation attempts to encode invalid UTF-8 sequences
    # Known culprits: at least some versions of macOS (confirmed Snow Leopard and Yosemite)
    try_run(mbrtowc_invalid_utf8_exit mbrtowc_invalid_utf8_compiles ${CMAKE_CURRENT_BINARY_DIR}
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/checks/mbrtowc_invalid_utf8.cpp")
    IF ("${mbrtowc_invalid_utf8_compiles}" AND ("${mbrtowc_invalid_utf8_exit}" EQUAL 1))
        SET(HAVE_BROKEN_MBRTOWC_UTF8 1)
    ENDIF()
ENDIF()
