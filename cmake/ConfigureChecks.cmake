# The following defines affect the environment configuration tests are run in:
# CMAKE_REQUIRED_DEFINITIONS, CMAKE_REQUIRED_FLAGS, CMAKE_REQUIRED_LIBRARIES,
# and CMAKE_REQUIRED_INCLUDES
# `wcstod_l` is a GNU-extension, sometimes hidden behind GNU-related defines.
# This is the case for at least Cygwin and Newlib.
LIST(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE=1)

IF(APPLE)
    INCLUDE(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-Werror=unguarded-availability" REQUIRES_UNGUARDED_AVAILABILITY)
    IF(REQUIRES_UNGUARDED_AVAILABILITY)
        LIST(APPEND CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} "-Werror=unguarded-availability")
    ENDIF()
ENDIF()

# Try using CMake's own logic to locate curses/ncurses
FIND_PACKAGE(Curses)
IF(NOT ${CURSES_FOUND})
    # CMake has trouble finding platform-specific system libraries
    # installed to multiarch paths (e.g. /usr/lib/x86_64-linux-gnu)
    # if not symlinked or passed in as a manual define.
    MESSAGE("Falling back to pkg-config for (n)curses detection")
    INCLUDE(FindPkgConfig)
    PKG_SEARCH_MODULE(CURSES REQUIRED ncurses curses)
    SET(CURSES_CURSES_LIBRARY ${CURSES_LIBRARIES})
    SET(CURSES_LIBRARY ${CURSES_LIBRARIES})
ENDIF()

# Get threads.
set(THREADS_PREFER_PTHREAD_FLAG ON)
# FindThreads < 3.4.0 doesn't work for C++-only projects
IF(CMAKE_VERSION VERSION_LESS 3.4.0)
    ENABLE_LANGUAGE(C)
ENDIF()
FIND_PACKAGE(Threads REQUIRED)

# Detect WSL. Does not match against native Windows/WIN32.
if (CMAKE_HOST_SYSTEM_VERSION MATCHES ".*-Microsoft")
  SET(WSL 1)
endif()

# Set up the config.h file.
SET(PACKAGE_NAME "fish")
SET(PACKAGE_TARNAME "fish")
INCLUDE(CheckCXXSymbolExists)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckStructHasMember)
INCLUDE(CheckCXXSourceCompiles)
INCLUDE(CheckTypeSize)
INCLUDE(CMakePushCheckState)
CHECK_CXX_SYMBOL_EXISTS(backtrace_symbols execinfo.h HAVE_BACKTRACE_SYMBOLS)
CHECK_CXX_SYMBOL_EXISTS(clock_gettime time.h HAVE_CLOCK_GETTIME)
CHECK_CXX_SYMBOL_EXISTS(ctermid_r stdio.h HAVE_CTERMID_R)
CHECK_STRUCT_HAS_MEMBER("struct dirent" d_type dirent.h HAVE_STRUCT_DIRENT_D_TYPE LANGUAGE CXX)
CHECK_CXX_SYMBOL_EXISTS(dirfd "sys/types.h;dirent.h" HAVE_DIRFD)
CHECK_INCLUDE_FILE_CXX(execinfo.h HAVE_EXECINFO_H)
CHECK_CXX_SYMBOL_EXISTS(flock sys/file.h HAVE_FLOCK)
# futimens is new in OS X 10.13 but is a weak symbol.
# Don't assume it exists just because we can link - it may be null.
CHECK_CXX_SYMBOL_EXISTS(futimens sys/stat.h HAVE_FUTIMENS)
CHECK_CXX_SYMBOL_EXISTS(futimes sys/time.h HAVE_FUTIMES)
CHECK_CXX_SYMBOL_EXISTS(getifaddrs ifaddrs.h HAVE_GETIFADDRS)
CHECK_CXX_SYMBOL_EXISTS(getpwent pwd.h HAVE_GETPWENT)
CHECK_CXX_SYMBOL_EXISTS(getrusage sys/resource.h HAVE_GETRUSAGE)
CHECK_CXX_SYMBOL_EXISTS(gettext libintl.h HAVE_GETTEXT)
CHECK_CXX_SYMBOL_EXISTS(killpg "sys/types.h;signal.h" HAVE_KILLPG)
CHECK_CXX_SYMBOL_EXISTS(lrand48_r stdlib.h HAVE_LRAND48_R)
# mkostemp is in stdlib in glibc and FreeBSD, but unistd on macOS
CHECK_CXX_SYMBOL_EXISTS(mkostemp "stdlib.h;unistd.h" HAVE_MKOSTEMP)
SET(HAVE_CURSES_H ${CURSES_HAVE_CURSES_H})
SET(HAVE_NCURSES_CURSES_H ${CURSES_HAVE_NCURSES_CURSES_H})
SET(HAVE_NCURSES_H ${CURSES_HAVE_NCURSES_H})
IF(HAVE_CURSES_H)
    CHECK_INCLUDE_FILES("curses.h;term.h" HAVE_TERM_H)
ENDIF()
IF(NOT HAVE_TERM_H)
    CHECK_INCLUDE_FILE_CXX("ncurses/term.h" HAVE_NCURSES_TERM_H)
ENDIF()
CHECK_INCLUDE_FILE_CXX(siginfo.h HAVE_SIGINFO_H)
CHECK_INCLUDE_FILE_CXX(spawn.h HAVE_SPAWN_H)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_ctime_nsec "sys/stat.h" HAVE_STRUCT_STAT_ST_CTIME_NSEC
    LANGUAGE CXX)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_mtimespec.tv_nsec "sys/stat.h"
    HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC LANGUAGE CXX)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_mtim.tv_nsec "sys/stat.h" HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
    LANGUAGE CXX)
CHECK_CXX_SYMBOL_EXISTS(sys_errlist stdio.h HAVE_SYS_ERRLIST)
CHECK_INCLUDE_FILE_CXX(sys/ioctl.h HAVE_SYS_IOCTL_H)
CHECK_INCLUDE_FILE_CXX(sys/select.h HAVE_SYS_SELECT_H)
CHECK_INCLUDE_FILES("sys/types.h;sys/sysctl.h" HAVE_SYS_SYSCTL_H)
CHECK_INCLUDE_FILE_CXX(termios.h HAVE_TERMIOS_H) # Needed for TIOCGWINSZ

CHECK_CXX_SYMBOL_EXISTS(wcscasecmp wchar.h HAVE_WCSCASECMP)
CHECK_CXX_SYMBOL_EXISTS(wcsdup wchar.h HAVE_WCSDUP)
CHECK_CXX_SYMBOL_EXISTS(wcslcpy wchar.h HAVE_WCSLCPY)
CHECK_CXX_SYMBOL_EXISTS(wcsncasecmp wchar.h HAVE_WCSNCASECMP)
CHECK_CXX_SYMBOL_EXISTS(wcsndup wchar.h HAVE_WCSNDUP)

# These are for compatibility with Solaris 10, which places the following
# in the std namespace.
IF(NOT HAVE_WCSNCASECMP)
    CHECK_CXX_SYMBOL_EXISTS(std::wcscasecmp wchar.h HAVE_STD__WCSCASECMP)
ENDIF()
IF(NOT HAVE_WCSDUP)
    CHECK_CXX_SYMBOL_EXISTS(std::wcsdup wchar.h HAVE_STD__WCSDUP)
ENDIF()
IF(NOT HAVE_WCSNCASECMP)
    CHECK_CXX_SYMBOL_EXISTS(std::wcsncasecmp wchar.h HAVE_STD__WCSNCASECMP)
ENDIF()

# `xlocale.h` is required to find `wcstod_l` in `wchar.h` under FreeBSD,
# but it's not present under Linux.
CHECK_INCLUDE_FILES("xlocale.h" HAVE_XLOCALE_H)
IF(HAVE_XLOCALE_H)
    LIST(APPEND WCSTOD_L_INCLUDES "xlocale.h")
ENDIF()
LIST(APPEND WCSTOD_L_INCLUDES "wchar.h")
CHECK_CXX_SYMBOL_EXISTS(wcstod_l "${WCSTOD_L_INCLUDES}" HAVE_WCSTOD_L)

CHECK_CXX_SYMBOL_EXISTS(_sys_errs stdlib.h HAVE__SYS__ERRS)

CMAKE_PUSH_CHECK_STATE()
SET(CMAKE_EXTRA_INCLUDE_FILES termios.h sys/ioctl.h)
CHECK_TYPE_SIZE("struct winsize" STRUCT_WINSIZE LANGUAGE CXX)
CHECK_CXX_SYMBOL_EXISTS("TIOCGWINSZ" "termios.h;sys/ioctl.h" HAVE_TIOCGWINSZ)
IF(STRUCT_WINSIZE GREATER -1 AND HAVE_TIOCGWINSZ EQUAL 1)
  SET(HAVE_WINSIZE 1)
ENDIF()
CMAKE_POP_CHECK_STATE()

CHECK_TYPE_SIZE("wchar_t[8]" WCHAR_T_BITS LANGUAGE CXX)

# Solaris, NetBSD and X/Open-conforming systems have a fixed-args tparm
SET(TPARM_INCLUDES)
IF(HAVE_NCURSES_H)
  SET(TPARM_INCLUDES "${TPARM_INCLUDES}#include <ncurses.h>\n")
ELSEIF(HAVE_NCURSES_CURSES_H)
  SET(TPARM_INCLUDES "${TPARM_INCLUDES}#include <ncurses/curses.h>\n")
ELSE()
  SET(TPARM_INCLUDES "${TPARM_INCLUDES}#include <curses.h>\n")
ENDIF()

IF(HAVE_TERM_H)
  SET(TPARM_INCLUDES "${TPARM_INCLUDES}#include <term.h>\n")
ELSEIF(HAVE_NCURSES_TERM_H)
  SET(TPARM_INCLUDES "${TPARM_INCLUDES}#include <ncurses/term.h>\n")
ENDIF()

CMAKE_PUSH_CHECK_STATE()
LIST(APPEND CMAKE_REQUIRED_LIBRARIES ${CURSES_LIBRARY})
CHECK_CXX_SOURCE_COMPILES("
${TPARM_INCLUDES}

int main () {
  tparm( \"\" );
}
"
  TPARM_TAKES_VARARGS
)
IF(NOT TPARM_TAKES_VARARGS)
  CHECK_CXX_SOURCE_COMPILES("
${TPARM_INCLUDES}
#define TPARM_VARARGS

int main () {
  tparm( \"\" );
}
"
    TPARM_TAKES_VARARGS_WITH_VARARGS
    )
  IF(NOT TPARM_TAKES_VARARGS)
    SET(TPARM_SOLARIS_KLUDGE 1)
  ELSE()
    SET(TPARM_VARARGS 1)
  ENDIF()
ENDIF()
CMAKE_POP_CHECK_STATE()

# Work around the fact that cmake does not propagate the language standard flag into
# the CHECK_CXX_SOURCE_COMPILES function. See CMake issue #16456.
# Ensure we do this after the FIND_PACKAGE calls which use C, and will error on a C++
# standards flag.
# Also see https://github.com/fish-shell/fish-shell/issues/5865
IF(NOT POLICY CMP0067)
  LIST(APPEND CMAKE_REQUIRED_FLAGS "${CMAKE_CXX${CMAKE_CXX_STANDARD}_EXTENSION_COMPILE_OPTION}")
ENDIF()

CHECK_CXX_SOURCE_COMPILES("
#include <memory>

int main () {
  std::unique_ptr<int> foo = std::make_unique<int>();
}
"
  HAVE_STD__MAKE_UNIQUE
)

FIND_PROGRAM(SED sed)

CHECK_CXX_SOURCE_COMPILES("
#include <atomic>
#include <cstdint>
std::atomic<uint64_t> x;
int main() {
   return x;
}"
LIBATOMIC_NOT_NEEDED)
IF (NOT LIBATOMIC_NOT_NEEDED)
    SET(ATOMIC_LIBRARY "atomic")
ENDIF()
