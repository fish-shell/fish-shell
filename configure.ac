#
# This file is the main build configuration file for fish. It is used
# to determine your systems capabilities, and tries to adapt fish to
# take maximum advantage of the services your system offers.
#
# Process this file using the 'autoconf' command to produce a working
# configure script, which should in turn be executed in order to
# configure the build process.
#

m4_syscmd([build_tools/git_version_gen.sh 2>/dev/null])

AC_INIT(fish,
        m4_esyscmd([cut -f 3 -d ' ' FISH-BUILD-VERSION-FILE | tr -d '\n']),
        fish-users@lists.sf.net)

conf_arg=$@

#
# List of output variables produced by this configure script
#

AC_SUBST(docdir)
AC_SUBST(HAVE_GETTEXT)
AC_SUBST(HAVE_DOXYGEN)
AC_SUBST(LDFLAGS_FISH)
AC_SUBST(LIBS_FISH)
AC_SUBST(LIBS_FISH_INDENT)
AC_SUBST(LIBS_FISH_PAGER)
AC_SUBST(LIBS_FISHD)
AC_SUBST(LIBS_MIMEDB)
AC_SUBST(localedir)
AC_SUBST(optbindirs)
AC_SUBST(prefix)


#
# If needed, run autoconf to regenerate the configure file
#
# This makes sure that after running autoconf once to create the first
# version of configure, we never again need to worry about manually
# running autoconf to handle an updates configure.ac.
#

AC_MSG_CHECKING([if autoconf needs to be run])
if test configure -ot configure.ac; then
  AC_MSG_RESULT([yes])
  if which autoconf >/dev/null; then
    # No need to provide any error messages if autoconf fails, the
    # shell and autconf should take care of that themselves
    AC_MSG_NOTICE([running autoconf])
    if autoconf; then
      ./configure "$@"
      exit
    fi
    exit 1
  else
    AC_MSG_ERROR(
      [cannot find the autoconf program in your path.
This program needs to be run whenever the configure.ac file is modified.
Please install it and try again.]
    )
  fi
else
  AC_MSG_RESULT([no])
fi


#
# If needed, run autoheader to regenerate config.h.in
#
# This makes sure we never ever have to run autoheader manually. It
# will be run whenever needed automatically.
#

AC_MSG_CHECKING([if autoheader needs to be run])
if test ! -f ./config.h.in -o config.h.in -ot configure.ac; then
  AC_MSG_RESULT([yes])
  if which autoheader >/dev/null; then
    AC_MSG_NOTICE([running autoheader])
    autoheader || exit 1
  else
    AC_MSG_ERROR(
      [cannot find the autoheader program in your path.
This program needs to be run whenever the configure.ac file is modified.
Please install it and try again.]
    )
  fi
else
  AC_MSG_RESULT([no])
fi

#
# Set up various programs needed for install
# Note AC_PROG_CXX sets CXXFLAGS if not set, which we want
# So ensure this happens before we modify CXXFLAGS below
#

AC_PROG_CXX([g++ c++])
AC_PROG_CPP
AC_PROG_INSTALL

echo "CXXFLAGS: $CXXFLAGS"

#
# Detect directories which may contain additional headers, libraries
# and commands. This needs to be done early - before Autoconf starts
# to mess with CFLAGS and all the other environemnt variables.
#
# This mostly helps OS X users, since fink usually installs out of
# tree and doesn't update CFLAGS.
#
# It also helps FreeBSD which puts libiconv in /usr/local/lib

for i in /usr/pkg /sw /opt /opt/local /usr/local; do

  AC_MSG_CHECKING([for $i/include include directory])
  if test -d $i/include; then
    AC_MSG_RESULT(yes)
    CXXFLAGS="$CXXFLAGS -I$i/include/"
    CFLAGS="$CFLAGS -I$i/include/"
  else
  AC_MSG_RESULT(no)
  fi

  AC_MSG_CHECKING([for $i/lib library directory])
  if test -d $i/lib; then
    AC_MSG_RESULT(yes)
    LDFLAGS="$LDFLAGS -L$i/lib/"
  else
    AC_MSG_RESULT(no)
  fi

  AC_MSG_CHECKING([for $i/bin command directory])
  if test -d $i/bin; then
    AC_MSG_RESULT(yes)
    optbindirs="$optbindirs $i/bin"
  else
    AC_MSG_RESULT(no)
  fi

done


#
# Tell autoconf to create config.h header
#
AC_CONFIG_HEADERS(config.h)


#
# This adds markup to the code that results in a few extra compile
# time checks on recent GCC versions. It helps stop a few common bugs.
#

AH_BOTTOM([#if __GNUC__ >= 3
#define __warn_unused __attribute__ ((warn_unused_result))
#define __sentinel __attribute__ ((sentinel))
#else
#define __warn_unused
#define __sentinel
#endif])


#
# Optionally drop gettext support
#

AC_ARG_WITH(
  gettext,
  AS_HELP_STRING(
    [--without-gettext],
    [do not translate messages, even if gettext is available]
  ),
  [local_gettext=$withval],
  [local_gettext=check]
)

AS_IF([test x$local_gettext != xno],
  [ AC_CHECK_PROGS( [found_msgfmt], [msgfmt], [no] )
    if test x$found_msgfmt != xno; then
      AC_DEFINE([USE_GETTEXT],[1],[Perform string translations with gettext])
    elif test "x$local_gettext" != "xcheck" ; then
      AC_MSG_FAILURE([--with-gettext was given, but the msgfmt program could not be found])
    else
      local_gettext=no
    fi
   ],
)

#
# Build/clean the documentation only if Doxygen is available
#

doxygen_minimum=1.5

AC_ARG_WITH(
  doxygen,
  AS_HELP_STRING(
    [--with-doxygen],
    [use Doxygen to regenerate documentation]
  ),
  [use_doxygen=$withval],
  [use_doxygen=auto]
)

AS_IF([test "$use_doxygen" != "no"],
  [
    AC_CHECK_PROGS([found_doxygen], [doxygen], [no])
    if test "$found_doxygen" != no; then
      # test version
      AC_MSG_CHECKING([the doxygen version])
      doxygen_version=`doxygen --version 2>/dev/null`
      AC_MSG_RESULT([$doxygen_version])
      AS_VERSION_COMPARE([$doxygen_version], [$doxygen_minimum],
        [ if test "$use_doxygen" = auto; then
            AC_MSG_WARN([doxygen version $doxygen_version found, but $doxygen_minimum required])
            HAVE_DOXYGEN=0
          else
            AC_MSG_FAILURE([doxygen version $doxygen_version found, but $doxygen_minimum required])
          fi
        ],
        [HAVE_DOXYGEN=1], [HAVE_DOXYGEN=1])
    elif test "$use_doxygen" != auto; then
      AC_MSG_FAILURE([--with-doxygen was given, but the doxygen program could not be found])
    else
      HAVE_DOXYGEN=0
    fi
  ],
)

#
# Try to enable large file support. This will make sure that on systems
# where off_t can be either 32 or 64 bit, the latter size is used. On
# other systems, this should do nothing. (Hopefully)
#

CXXFLAGS="$CXXFLAGS -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64"

#
# If we are using gcc, set some flags that increase the odds of the
# compiler producing a working binary...
#

if test "$GCC" = yes; then

  #
  # -fno-optimize-sibling-calls seems to work around a bug where
  # sending a SIGWINCH to fish on NetBSD 3.0 causes fish to exit when
  # compiled with GCC 3.3.3. This is probably either a compiler bug
  # or a libc bug, but adding this flag seems to fix things for
  # now. Long term, the real problem should be tracked down and
  # truly fixed, at which point we can remove this silly flag. This
  # bug has been verified to not exist on Linux using GCC 3.3.3.
  #

  GCC_VERSION=$($CC -dumpversion)
  GCC_VERSION_MAJOR=$(echo $GCC_VERSION | cut -d'.' -f1)
  GCC_VERSION_MINOR=$(echo $GCC_VERSION | cut -d'.' -f2)
  GCC_VERSION_PATCH=$(echo $GCC_VERSION | cut -d'.' -f3)

  if test "$GCC_VERSION_MAJOR" -le 3; then
    if test 0"$GCC_VERSION_MINOR" -le 3; then
      if test 0"$GCC_VERSION_PATCH" -le 3; then
        CXXFLAGS="$CXXFLAGS -fno-optimize-sibling-calls"
      fi
    fi
  fi

  # fish does not use exceptions
  # Disabling exceptions saves about 20% (!) of the compiled code size
  CXXFLAGS="$CXXFLAGS -fno-exceptions"


  #
  # -Wall is there to keep me on my toes
  #

  # Some day...
  CXXFLAGS="$CXXFLAGS -Wall"

  #
  # This is needed in order to get the really cool backtraces on Linux
  #

  if test `uname` != "Darwin"; then
    LDFLAGS_FISH="$LDFLAGS_FISH -rdynamic"
  fi

fi


#
# If we are compiling against glibc, set some flags to work around
# some rather stupid attempts to hide prototypes for *wprintf
# functions, as well as prototypes of various gnu extensions.
#

AC_MSG_CHECKING([if we are compiling against glibc])
AC_RUN_IFELSE(
  [
    AC_LANG_PROGRAM(
      [
        #include <stdlib.h>
        #ifdef __GLIBC__
        #define STATUS 0
        #else
        #define STATUS 1
        #endif
      ],
      [
        return STATUS;
      ]
    )
  ],
  [glibc=yes],
  [glibc=no]
)

if test "$glibc" = yes; then
  AC_MSG_RESULT(yes)

  #
  # This gives us access to prototypes for gnu extensions and C99
  # functions if we are compiling agains glibc. All GNU extensions
  # that are used must have a fallback implementation available in
  # fallback.h, in order to keep fish working on non-gnu platforms.
  #

  CFLAGS="$CFLAGS -D_GNU_SOURCE=1 -D_ISO99_SOURCE=1"
else
  AC_MSG_RESULT(no)
fi


#
# Test cpu for special handling of ppc
#
# This is used to skip use of tputs on ppc systems, since it seemed to
# be broken, at least on older debin-based systems. This is obviously
# not the right way to to detect whether this workaround should be
# used, since it catches far to many systems, but I do not have the
# hardware available to narrow this problem down, and in practice, it
# seems that tputs is never really needed.
#

AC_CANONICAL_TARGET

if test $target_cpu = powerpc; then
  AC_DEFINE([TPUTS_KLUDGE],[1],[Evil kludge to get Power based machines to work])
fi


#
# Solaris-specific flags go here
#

AC_MSG_CHECKING([if we are under Solaris])
case $target_os in
  solaris*)
    AC_DEFINE( __EXTENSIONS__, 1, [Macro to enable additional prototypes under Solaris])
    AC_MSG_RESULT(yes)
    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
esac

# Check for Solaris curses tputs having fixed length parameter list.
AC_MSG_CHECKING([if we are using non varargs tparm.])
AC_COMPILE_IFELSE(
  [
    AC_LANG_PROGRAM(
      [
        #include <curses.h>
        #include <term.h>
      ],
      [
        tparm( "" );
      ]
    )
  ],
  [tparm_solaris_kludge=no],
  [tparm_solaris_kludge=yes]
)
if test "x$tparm_solaris_kludge" = "xyes"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(
    [TPARM_SOLARIS_KLUDGE],
    [1],
    [Define to 1 if tparm accepts a fixed amount of paramters.]
  )
else
  AC_MSG_RESULT(no)
fi


#
# BSD-specific flags go here
#

AC_MSG_CHECKING([if we are under BSD])
case $target_os in
  *bsd*)
    AC_DEFINE( __BSD_VISIBLE, 1, [Macro to enable additional prototypes under BSD])
    AC_DEFINE( _NETBSD_SOURCE, 1, [Macro to enable additional prototypes under BSD])
    AC_MSG_RESULT(yes)
    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
esac


#
# Set up PREFIX and related preprocessor symbols. Fish needs to know
# where it will be installed. One of the reasons for this is so that
# it can make sure the fish installation directory is in the path
# during startup.
#

if [[ "$prefix" = NONE ]]; then
  prefix=/usr/local
fi


#
# Set up the directory where the documentation files should be
# installed
#

AC_ARG_VAR( [docdir], [Documentation direcotry] )

if test -z $docdir; then
  docdir=$datadir/doc/fish
else
  docdir=$docdir
fi


#
# Set up locale directory. This is where the .po files will be
# installed.
#

localedir=$datadir/locale


#
# See if Linux procfs is present. This is used to get extra
# information about running processes.
#

AC_CHECK_FILES([/proc/self/stat])


#
# This is ued to tell the wgetopt library to translate strings. This
# way wgetopt can be dropped into any project without requiring i18n.
#

AC_DEFINE(
  [HAVE_TRANSLATE_H],
  [1],
  [Define to 1 if the wgettext function should be used for translating strings.]
)


#
# Check presense of various libraries. This is done on a per-binary
# level, since including various extra libraries in all binaries only
# because thay are used by some of them can cause extra bloat and
# slower compiles when developing fish.
#

# Check for os dependant libraries for all binaries.
LIBS_COMMON=$LIBS
LIBS=""
AC_SEARCH_LIBS( connect, socket, , [AC_MSG_ERROR([Cannot find the socket library, needed to build this package.] )] )
AC_SEARCH_LIBS( nanosleep, rt, , [AC_MSG_ERROR([Cannot find the rt library, needed to build this package.] )] )
AC_SEARCH_LIBS( pthread_create, pthread, , [AC_MSG_ERROR([Cannot find the pthread library, needed to build this package.] )] )
AC_SEARCH_LIBS( setupterm, [ncurses curses], , [AC_MSG_ERROR([Could not find a curses implementation, needed to build fish. If this is Linux, try running 'sudo apt-get install libncurses5-dev' or 'sudo yum install ncurses-devel'])] )
AC_SEARCH_LIBS( [nan], [m], [AC_DEFINE( [HAVE_NAN], [1], [Define to 1 if you have the nan function])] )
LIBS_SHARED=$LIBS
LIBS=$LIBS_COMMON

#
# Check for libraries needed by fish.
#

LIBS_COMMON=$LIBS
LIBS="$LIBS_SHARED"
if test x$local_gettext != xno; then
  AC_SEARCH_LIBS( gettext, intl,,)
fi

# Check for libiconv_open if we can't find iconv_open. Silly OS X does
# weird macro magic for the sole purpose of amusing me.
AC_SEARCH_LIBS( iconv_open, iconv, , [AC_SEARCH_LIBS( libiconv_open, iconv, , [AC_MSG_ERROR([Could not find an iconv implementation, needed to build fish])] )] )

LIBS_FISH=$LIBS
LIBS=$LIBS_COMMON

#
# Check for libraries needed by fish_indent.
#

LIBS_COMMON=$LIBS
LIBS="$LIBS_SHARED"
if test x$local_gettext != xno; then
  AC_SEARCH_LIBS( gettext, intl,,)
fi
LIBS_FISH_INDENT=$LIBS
LIBS=$LIBS_COMMON

#
# Check for libraries needed by fish_pager.
#

LIBS_COMMON=$LIBS
LIBS="$LIBS_SHARED"
if test x$local_gettext != xno; then
  AC_SEARCH_LIBS( gettext, intl,,)
fi
AC_SEARCH_LIBS( iconv_open, iconv, , [AC_SEARCH_LIBS( libiconv_open, iconv, , [AC_MSG_ERROR([Could not find an iconv implementation, needed to build fish])] )] )
LIBS_FISH_PAGER=$LIBS
LIBS=$LIBS_COMMON

#
# Check for libraries needed by fishd.
#

LIBS_COMMON=$LIBS
LIBS="$LIBS_SHARED"
if test x$local_gettext != xno; then
  AC_SEARCH_LIBS( gettext, intl,,)
fi
AC_SEARCH_LIBS( iconv_open, iconv, , [AC_SEARCH_LIBS( libiconv_open, iconv, , [AC_MSG_ERROR([Could not find an iconv implementation, needed to build fish])] )] )
LIBS_FISHD=$LIBS
LIBS=$LIBS_COMMON

#
# Check for libraries needed by mimedb.
#

LIBS_COMMON=$LIBS
LIBS="$LIBS_SHARED"
if test x$local_gettext != xno; then
  AC_SEARCH_LIBS( gettext, intl,,)
fi
LIBS_MIMEDB=$LIBS
LIBS=$LIBS_COMMON


#
# Check presense of various header files
#

AC_CHECK_HEADERS([getopt.h termios.h sys/resource.h term.h ncurses/term.h ncurses.h curses.h stropts.h siginfo.h sys/select.h sys/ioctl.h execinfo.h spawn.h sys/sysctl.h])

if test x$local_gettext != xno; then
  AC_CHECK_HEADERS([libintl.h])
fi

AC_CHECK_HEADER(
  [regex.h],
  [
    AC_DEFINE(
      [HAVE_REGEX_H],
      [1],
      [Define to 1 if you have the <regex.h> header file.]
    )
  ],
  [AC_MSG_ERROR([Could not find the header regex.h, needed to build fish])]
)


#
# On some platforms (Solaris 10) adding -std=c99 in turn requires that
# _POSIX_C_SOURCE be defined to 200112L otherwise several
# POSIX-specific, non-ISO-C99 types/prototypes are made unavailable
# e.g. siginfo_t. Defining _XOPEN_SOURCE to 600 is compatible with
# the _POSIX_C_SOURCE value and provides a little assurance that
# extension functions' prototypes are available, e.g. killpg().
#
# Some other platforms (OS X), will remove types/prototypes/macros
# e.g. SIGWINCH if either _POSIX_C_SOURCE or _XOPEN_SOURCE is defined.
#
# This test adds these macros only if they enable a program that uses
# both Posix and non-standard features to compile, and that program
# does not compile without these macros.
#
# We try to make everyone happy.
#
# The ordering of the various autoconf tests is very critical as well:
#
# * This test needs to be run _after_ header detection tests, so that
# the proper headers are included.
#
# * This test needs to be run _before_ testing for the presense of any
# prototypes or other language functinality.
#
# * This test should be (but does not need to be) run after the
# conditional definition of __EXTENSIONS__, to avoid redundant tests.
#

XCFLAGS="$CXXFLAGS"

echo checking how to use -D_XOPEN_SOURCE=600 and -D_POSIX_C_SOURCE=200112L...
local_found_posix_switch=no

for i in "" "-D_POSIX_C_SOURCE=200112L" "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L"; do

  AC_MSG_CHECKING( if switches \"$i\" works)
  CFLAGS="$XCFLAGS $i"

  #
  # Try to run this program, which should test various extensions
  # and Posix functionality. If this program works, then everything
  # should work. Hopefully.
  #

  AC_TRY_LINK(
    [
      #include <stdlib.h>
      #include <stdio.h>
      #include <sys/types.h>

      /* POSIX, C89 and C99: POSIX extends this header.
       * For: kill(), killpg(), siginfo_t, sigset_t,
       * struct sigaction, sigemptyset(), sigaction(),
       * SIGIO and SIGWINCH.  */
      #include <signal.h>

      #ifdef HAVE_SIGINFO_H
      /* Neither POSIX, C89 nor C99: Solaris-specific (others?).
       * For: siginfo_t (also defined by signal.h when in
       * POSIX/extensions mode). */
      #include <siginfo.h>
      #endif

      #ifdef HAVE_SYS_IOCTL_H
      /* As above (under at least Linux and FreeBSD). */
      #include <sys/ioctl.h>
      #endif
    ],
    [
      /* Avert high-level optimisation, by making the program's
       * return value depend on all tested identifiers. */
      long ret = 0;
      /* POSIX only: might be unhidden by _POSIX_C_SOURCE. */
      struct sigaction sa;
      sigset_t ss;
      siginfo_t info;
      ret += (long)(void *)&info + kill( 0, 0 ) +
        sigaction( 0, &sa, 0 ) + sigemptyset( &ss );
      /* Extended-POSIX: might be unhidden by _XOPEN_SOURCE. */
      ret += killpg( 0, 0 );
      /* Non-standard: might be hidden by the macros. */
      {
        struct winsize termsize;
        ret += (long)(void *)&termsize;
        ret += SIGWINCH + TIOCGWINSZ + SIGIO;
      }
      return ret;

    ],
    local_cv_use__posix_c_source=yes,
    local_cv_use__posix_c_source=no,
  )

  if test x$local_cv_use__posix_c_source = xyes; then
    AC_MSG_RESULT( yes )
    local_found_posix_switch=yes
    break;
  else
    AC_MSG_RESULT( no )
  fi

done

#
# We didn't find any combination of switches that worked - revert to
# no switches and hope that the fallbacks work.  A warning will be
# printed at the end of the configure script.
#

if test ! x$local_found_posix_switch = xyes; then
  CFLAGS="$XCFLAGS"
fi


#
# Check for presense of various functions used by fish
#

AC_CHECK_FUNCS( wcsdup wcsndup wcslen wcscasecmp wcsncasecmp fwprintf )
AC_CHECK_FUNCS( futimes wcwidth wcswidth wcstok fputwc fgetwc )
AC_CHECK_FUNCS( wcstol wcslcat wcslcpy lrand48_r killpg )
AC_CHECK_FUNCS( backtrace backtrace_symbols sysconf getifaddrs )

if test x$local_gettext != xno; then
  AC_CHECK_FUNCS( gettext dcgettext )

#
# The Makefile also needs to know if we have gettext, so it knows if
# the translations should be installed.
#
  AC_CHECK_FUNC( gettext, HAVE_GETTEXT=1, HAVE_GETTEXT=0 )
fi

#
# Here follows a list of small programs used to test for various
# features that Autoconf doesn't tell us about
#


#
# Check if realpath accepts null for its second argument
#

AC_MSG_CHECKING([if realpath accepts null for its second argument])
AC_RUN_IFELSE(
  [
    AC_LANG_PROGRAM(
      [
        #include <limits.h>
        #include <errno.h>
        #include <stdlib.h>
      ],
      [
        int status;
        char *res;
        res = realpath( "somefile", 0 );
        status = !(res != 0 || errno == ENOENT);
        exit( status );
      ]
    )
  ],
  [have_realpath_null=yes],
  [have_realpath_null=no]
)

if test "$have_realpath_null" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(
    [HAVE_REALPATH_NULL],
    [1],
    [Define to 1 if realpath accepts null for its second argument.]
  )
else
  AC_MSG_RESULT(no)
fi


#
# Check if struct winsize and TIOCGWINSZ exist
#

AC_MSG_CHECKING([if struct winsize and TIOCGWINSZ exist])
AC_LINK_IFELSE(
  [
    AC_LANG_PROGRAM(
      [
        #ifdef HAVE_TERMIOS_H
        #include <termios.h>
        #endif

        #ifdef HAVE_SYS_IOCTL_H
        #include <sys/ioctl.h>
        #endif
      ],
      [
        struct winsize termsize = {0};
        TIOCGWINSZ;
      ]
    )
  ],
  [
    AC_MSG_RESULT(yes);
    AC_DEFINE([HAVE_WINSIZE], [1], [Define to 1 if the winsize struct and TIOCGWINSZ macro exist])
  ],
  [
    AC_MSG_RESULT(no)
  ]
)


#
# If we have a fwprintf in libc, test that it actually works. As of
# March 2006, it is broken under Dragonfly BSD.
#

if test "$ac_cv_func_fwprintf" = yes; then

  AC_MSG_CHECKING([if fwprintf is broken])
  AC_RUN_IFELSE(
    [
      AC_LANG_PROGRAM(
        [
          #include <stdlib.h>
          #include <stdio.h>
          #include <locale.h>
          #include <wchar.h>
        ],
        [
          setlocale( LC_ALL, "" );
          fwprintf( stderr, L"%ls%ls", L"", L"fish:" );
        ]
      )
    ],
    [
      AC_MSG_RESULT(no)
    ],
    [
      AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_BROKEN_FWPRINTF], [1], [Define to 1 one if the implemented fwprintf is broken])
    ]
  )

fi


# Check for _nl_msg_cat_cntr symbol
AC_MSG_CHECKING([for _nl_msg_cat_cntr symbol])
AC_TRY_LINK(
  [
    #if HAVE_LIBINTL_H
    #include <libintl.h>
    #endif
  ],
  [
    extern int  _nl_msg_cat_cntr;
    int tmp = _nl_msg_cat_cntr;
    exit(tmp);
  ],
  have__nl_msg_cat_cntr=yes,
  have__nl_msg_cat_cntr=no
)
if test "$have__nl_msg_cat_cntr" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(
    [HAVE__NL_MSG_CAT_CNTR],
    [1],
    [Define to 1 if the _nl_msg_cat_cntr symbol is exported.]
  )
else
  AC_MSG_RESULT(no)
fi

# Check for __environ symbol
AC_MSG_CHECKING([for __environ symbol])
AC_TRY_LINK(
  [
    #include <unistd.h>
  ],
  [
    extern char **__environ;
    char **tmp = __environ;
    exit(tmp!=0);
  ],
  have___environ=yes,
  have___environ=no
)
if test "$have___environ" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(
    [HAVE___ENVIRON],
    [1],
    [Define to 1 if the __environ symbol is exported.]
  )
else
  AC_MSG_RESULT(no)
fi

# Check if getopt_long exists and works
AC_MSG_CHECKING([if getopt_long exists and works])
AC_TRY_LINK(
  [
    #if HAVE_GETOPT_H
    #include <getopt.h>
    #endif
  ],
  [
    static struct option
      long_options[] =
      {
        0, 0, 0, 0
      }
    ;
    int opt = getopt_long( 0,
                 0,
                 0,
                 long_options,
                 0 );

  ],
  have_working_getopt_long=yes,
  have_working_getopt_long=no
)
if test "$have_working_getopt_long" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(
    [HAVE_WORKING_GETOPT_LONG],
    [1],
    [Define to 1 if getopt_long exists and works.]
  )
else
  AC_MSG_RESULT(no)
fi

# Check if del_curterm is broken - in that case we redefine
# del_curterm as a no-op, to avoid a double-free

AC_MSG_CHECKING([If del_curterm is broken])
case $target_os in
  *bsd*)
    AC_MSG_RESULT(yes)
    AC_DEFINE(
      [HAVE_BROKEN_DEL_CURTERM],
      [1],
      [del_curterm is broken, redefine it to a no-op to avoid a double-free bug]
    )
    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
esac

# Tell the world what we know
AC_CONFIG_FILES([Makefile fish.spec])
AC_OUTPUT

if test ! x$local_found_posix_switch = xyes; then
  echo "Can't find a combination of switches to enable common extensions like detecting window size."
  echo "Some fish features may be disabled."
fi

echo "fish is now configured."
echo "Use 'make' and 'make install' to build and install fish."

