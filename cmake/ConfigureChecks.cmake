# The following defines affect the environment configuration tests are run in:
# CMAKE_REQUIRED_DEFINITIONS, CMAKE_REQUIRED_FLAGS, CMAKE_REQUIRED_LIBRARIES,
# and CMAKE_REQUIRED_INCLUDES
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE=1)

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
