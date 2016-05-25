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

AC_PREREQ([2.60])
AC_INIT(fish,
        m4_esyscmd([cut -f 3 -d ' ' FISH-BUILD-VERSION-FILE | tr -d '\n']),
        fish-users@lists.sourceforge.net)

#
# List of output variables produced by this configure script
#

AC_SUBST(HAVE_GETTEXT)
AC_SUBST(HAVE_DOXYGEN)
AC_SUBST(LDFLAGS_FISH)
AC_SUBST(WCHAR_T_BITS)
AC_SUBST(EXTRA_PCRE2)


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
AC_PROG_INSTALL
AC_PROG_SED
AC_LANG(C++)
AC_USE_SYSTEM_EXTENSIONS

AC_CANONICAL_TARGET

echo "CXXFLAGS: $CXXFLAGS"

#
# Tell autoconf to create config.h header
#
AC_CONFIG_HEADERS(config.h)


#
# This adds markup to the code that results in a few extra compile
# time checks on recent GCC versions. It helps stop a few common bugs.
#

AH_BOTTOM([#if __GNUC__ >= 3
#ifndef __warn_unused
#define __warn_unused __attribute__ ((warn_unused_result))
#endif
#ifndef __sentinel
#define __sentinel __attribute__ ((sentinel))
#endif
#ifndef __packed
#define __packed __attribute__ ((packed))
#endif
#else
#define __warn_unused
#define __sentinel
#define __packed
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

doxygen_minimum=1.8.7

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
      dnl This requires autoconf 2.60 or newer
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
AC_SYS_LARGEFILE


# fish does not use exceptions
# Disabling exceptions saves about 20% (!) of the compiled code size
CXXFLAGS="$CXXFLAGS -fno-exceptions"


#
# -Wall is there to keep me on my toes
# But signed comparison warnings are way too aggressive
#

CXXFLAGS="$CXXFLAGS -Wall -Wno-sign-compare"

#
# This is needed in order to get the really cool backtraces on Linux
#
AC_MSG_CHECKING([for -rdynamic linker flag])
prev_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -rdynamic"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[]],[[]])],
               [
                AC_MSG_RESULT([yes])
                LDFLAGS_FISH="$LDFLAGS_FISH -rdynamic"
               ], [
                AC_MSG_RESULT([no])
                LDFLAGS_FISH="$LDFLAGS_FISH"
               ])
LDFLAGS="$prev_LDFLAGS"

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

# Disable curses macros that conflict with the STL
AC_DEFINE([NCURSES_NOMACROS], [1], [Define to 1 to disable ncurses macros that conflict with the STL])
AC_DEFINE([NOMACROS], [1], [Define to 1 to disable curses macros that conflict with the STL])

#
# Check presense of various libraries. This is done on a per-binary
# level, since including various extra libraries in all binaries only
# because thay are used by some of them can cause extra bloat and
# slower compiles when developing fish.
#

# Check for os dependant libraries for all binaries.
AC_SEARCH_LIBS( connect, socket, , [AC_MSG_ERROR([Cannot find the socket library, needed to build this package.] )] )
AC_SEARCH_LIBS( nanosleep, rt, , [AC_MSG_ERROR([Cannot find the rt library, needed to build this package.] )] )
AC_SEARCH_LIBS( shm_open, rt, , [AC_MSG_ERROR([Cannot find the rt library, needed to build this package.] )] )
AC_SEARCH_LIBS( pthread_create, pthread, , [AC_MSG_ERROR([Cannot find the pthread library, needed to build this package.] )] )
AC_SEARCH_LIBS( setupterm, [ncurses tinfo curses], , [AC_MSG_ERROR([Could not find a curses implementation, needed to build fish. If this is Linux, try running 'sudo apt-get install libncurses5-dev' or 'sudo yum install ncurses-devel'])] )
AC_SEARCH_LIBS( [nan], [m], [AC_DEFINE( [HAVE_NAN], [1], [Define to 1 if you have the nan function])] )
AC_SEARCH_LIBS( [dladdr], [dl] )

if test x$local_gettext != xno; then
  AC_SEARCH_LIBS( gettext, intl,,)
fi

#
# Check presense of various header files
#

AC_CHECK_HEADERS([getopt.h termios.h sys/resource.h term.h ncurses/term.h ncurses.h ncurses/curses.h curses.h stropts.h siginfo.h sys/select.h sys/ioctl.h execinfo.h spawn.h sys/sysctl.h])

if test x$local_gettext != xno; then
  AC_CHECK_HEADERS([libintl.h])
fi


#
# Get the size in bits of wchar_t, needed for configuring the pcre2 build
# and for code that #includes pcre2.h
#

AC_CHECK_SIZEOF(wchar_t)
WCHAR_T_BITS=`expr 8 \* $ac_cv_sizeof_wchar_t`
AC_DEFINE_UNQUOTED([WCHAR_T_BITS], [$WCHAR_T_BITS], [The size of wchar_t in bits.])

#
# Detect nanoseconds fields in struct stat
#
AC_CHECK_MEMBERS([struct stat.st_mtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec])

#
# Check for D_TYPE in dirent, only on BSD and Linux
#
AC_STRUCT_DIRENT_D_TYPE

#
# Check for presence of various functions used by fish
#

AC_CHECK_FUNCS( wcsndup )
AC_CHECK_FUNCS( futimes )
AC_CHECK_FUNCS( wcslcat wcslcpy lrand48_r killpg )
AC_CHECK_FUNCS( backtrace_symbols getifaddrs )
AC_CHECK_FUNCS( futimens clock_gettime )

AC_CHECK_DECL( [mkostemp], [ AC_CHECK_FUNCS([mkostemp]) ] )

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


# Check for _nl_msg_cat_cntr symbol
AC_MSG_CHECKING([for _nl_msg_cat_cntr symbol])
AC_TRY_LINK(
  [
    #if HAVE_LIBINTL_H
    #include <libintl.h>
    #endif
    #include <stdlib.h>
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


# Check for sys_errlist
AC_MSG_CHECKING([for sys_errlist array])
AC_TRY_LINK(
  [
   #include <stdio.h>
  ],
  [
    const char *p;
    p = sys_errlist[sys_nerr];
  ],
  have_sys_errlist=yes,
  have_sys_errlist=no
)
if test "$have_sys_errlist" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(
    [HAVE_SYS_ERRLIST],
    [1],
    [Define to 1 if the sys_errlist array is available.]
  )
else
  AC_MSG_RESULT(no)
fi

# Check for _sys_errs
AC_MSG_CHECKING([for _sys_errs array])
AC_TRY_LINK(
  [
   #include <string>
  ],
  [
    std::string p;
    extern const char _sys_errs[];
    extern const int _sys_index[];
    p = _sys_errs[_sys_index[0]];
  ],
  have__sys__errs=yes,
  have__sys__errs=no
)
if test "$have__sys__errs" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(
    [HAVE__SYS__ERRS],
    [1],
    [Define to 1 if the _sys_errs array is available.]
  )
else
  AC_MSG_RESULT(no)
fi

# Check for Solaris curses tputs having fixed length parameter list.
AC_MSG_CHECKING([if we are using non varargs tparm.])
AC_COMPILE_IFELSE(
  [
    AC_LANG_PROGRAM(
      [
	#if HAVE_NCURSES_H
	#include <ncurses.h>
	#elif HAVE_NCURSES_CURSES_H
	#include <ncurses/curses.h>
	#else
	#include <curses.h>
	#endif

	#if HAVE_TERM_H
	#include <term.h>
	#elif HAVE_NCURSES_TERM_H
	#include <ncurses/term.h>
	#endif
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

pcre2_min_version=10.21
EXTRA_PCRE2=
AC_ARG_WITH(
  included-pcre2,
  AS_HELP_STRING(
    [--without-included-pcre2],
    [build against the system PCRE2 library instead of the bundled version]
  ),
  [included_pcre2=$withval],
  [included_pcre2=auto]
)

if test "x$included_pcre2" != "xyes"; then

  # test for pcre2-config
  # can use either pcre2-config or pkgconfig here but only implement the former for now
  AC_CHECK_PROG(PCRE2_CONFIG, pcre2-config, pcre2-config)

  if test "x$PCRE2_CONFIG" != "x"; then
    dnl AC_MSG_CHECKING([for $WCHAR_T_BITS-bit PCRE2])
    XLIBS="$LIBS"
    LIBS="$LIBS "`$PCRE2_CONFIG --libs$WCHAR_T_BITS 2>/dev/null`
    XCXXFLAGS="$CXXFLAGS"
    CXXFLAGS="$CXXFLAGS "`$PCRE2_CONFIG --cflags`

    # cheat a bit here. the exact library is determined by $WCHAR_T_BITS,
    # and so AC_CHECK_LIB won't work (can't use a variable as library name)
    # AC_SEARCH_LIBS will use the existing $LIBS flags with no additional library first
    AC_SEARCH_LIBS([pcre2_compile_$WCHAR_T_BITS], [],
      [ # pcre2 lib found, check for minimum version
        pcre2_version=`$PCRE2_CONFIG --version`
        AS_VERSION_COMPARE([$pcre2_version], [$pcre2_min_version],
          [ # version < minimum
            AC_MSG_NOTICE([system PCRE2 library version $pcre2_version, need $pcre2_min_version or later])
            if test "x$included_pcre2" = "xno"; then
              # complain about pcre2 version
              AC_MSG_ERROR([system PCRE2 library is too old, but --without-included-pcre2 was given.])
            else
              # use the internal version; undo changes to LIBS/CXXFLAGS
              included_pcre2=yes
              LIBS="$XLIBS"
              CXXFLAGS="$XCXXFLAGS"
            fi
          ],
          [ # version == minimum
            working_pcre2=yes
          ],
          [ # version > minimum
            working_pcre2=yes
          ]
        )
      ],
      [ # fail case; undo the changes to LIBS/CXXFLAGS
       working_pcre2=no
       LIBS="$XLIBS"
       CXXFLAGS="$XCXXFLAGS"
      ]
    )
  fi

  if test "x$working_pcre2" = "xyes"; then
    AC_MSG_NOTICE([using system PCRE2 library])
  else
    # pcre2 size wrong or pcre2-config not found
    # is it OK to use the included version?
    if test "x$included_pcre2" = "xno"; then
      # complain
      AC_MSG_ERROR([cannot find system pcre2-config, but --without-included-pcre2 was given.
Make sure pcre2-config is installed and available in PATH.
You may need to install the PCRE2 development library for your system.])
    else
      # use the internal version
      included_pcre2=yes
    fi
  fi
fi

# Re-test as value may have changed.
if test "x$included_pcre2" = "xyes"; then
  # Build configure/Makefile for pcre2
  AC_MSG_NOTICE([using included PCRE2 library])
  # unfortunately these get added to the global configuration
  ac_configure_args="$ac_configure_args --disable-pcre2-8 --enable-pcre2-$WCHAR_T_BITS --disable-shared"
  AC_CONFIG_SUBDIRS([pcre2-10.21])

  PCRE2_CXXFLAGS='-I$(PCRE2_DIR)/src'
  PCRE2_LIBS='-L$(PCRE2_LIBDIR) -lpcre2-$(PCRE2_WIDTH)'

  # Make the binary depend on the PCRE2 libraries so they get built
  EXTRA_PCRE2='$(PCRE2_LIB)'
  CXXFLAGS="$CXXFLAGS $PCRE2_CXXFLAGS"
  LIBS="$LIBS $PCRE2_LIBS"
fi

# Allow configurable extra directories.
AC_SUBST(extra_completionsdir)
AC_ARG_WITH([extra-completionsdir],
            AS_HELP_STRING([--with-extra-completionsdir=DIR],
                           [path for extra completions]),
            [extra_completionsdir=$withval],
            [extra_completionsdir='${datadir}/fish/vendor_completions.d'])

AC_SUBST(extra_functionsdir)
AC_ARG_WITH([extra_functionsdir],
            AS_HELP_STRING([--with-extra-functionsdir=DIR],
                           [path for extra functions]),
            [extra_functionsdir=$withval],
            [extra_functionsdir='${datadir}/fish/vendor_functions.d'])

AC_SUBST(extra_confdir)
AC_ARG_WITH([extra-confdir],
            AS_HELP_STRING([--with-extra-confdir=DIR],
                           [path for extra conf]),
            [extra_confdir=$withval],
            [extra_confdir='${datadir}/fish/vendor_conf.d'])

# Tell the world what we know.
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "fish is now configured."
echo "Use 'make' and 'make install' to build and install fish."

