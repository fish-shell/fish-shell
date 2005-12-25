# Process this file with autoconf to produce a configure script.
AC_INIT(fish,1.19.0,fish-users@lists.sf.net)

AC_CANONICAL_TARGET

if test $target_cpu = powerpc; then
   AC_DEFINE([TPUTS_KLUDGE],[1],[Evil kludge to get Power based machines to work])
fi

AC_DEFINE_UNQUOTED([CPU],[L"$target_cpu"],[CPU type])

AC_CONFIG_HEADERS(config.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
#AC_ISC_POSIX
#AC_PROG_MAKE_SET

AC_CHECK_PROG( has_doxygen, [doxygen], "true")

if ! test $has_doxygen = "true"; then
	echo Could not find the Doxygen program in your path.
	echo This program is needed to build fish.
	echo Please install it and try again.
	exit 1
fi

# Optionally drop xsel
AC_ARG_WITH( xsel, 
			 AC_HELP_STRING([--without-xsel],
             [do not build the xsel program needed for X clipboard integration]),
			 [xsel=$withval],
			 [xsel=with_xsel] )

if [[ "$xsel" = "with_xsel" ]]; then
    AC_SUBST( XSEL,[xsel-0.9.6/xsel])
    AC_SUBST( XSEL_MAN,[xsel.1x])
    AC_SUBST( XSEL_MAN_PATH,[xsel-0.9.6/xsel.1x])
else
    AC_SUBST( XSEL,[ ])
    AC_SUBST( XSEL_MAN,[ ])
    AC_SUBST( XSEL_MAN_PATH,[ ])
fi

if [[ "$prefix" = NONE ]]; then
	AC_DEFINE_UNQUOTED( [PREFIX], L"/usr/local", [Installation directory])
	AC_SUBST( PREFIX, /usr/local)
	export prefix=/usr/local
else
	AC_DEFINE_UNQUOTED( [PREFIX], L"$prefix", [Installation directory])
	AC_SUBST( PREFIX, [$prefix])
fi

AC_SUBST(fishdir,[/fish.d])
AC_SUBST(fishfile,[/fish])
AC_SUBST(fishinputfile,[/fish_inputrc])

AC_ARG_VAR( [docdir], [Documentation direcotry] )

if test -z $docdir; then
   AC_SUBST(docdir,[$datadir/doc/fish])
fi

AC_DEFINE_UNQUOTED( DOCDIR, [L"$(eval echo $docdir)"], [Documentation directory] )
AC_DEFINE_UNQUOTED( SYSCONFDIR, [L"$(eval echo $sysconfdir)"], [System configuration directory] )
AC_SUBST( SYSCONFDIR, ["$(eval echo $sysconfdir)"] )

# See if Linux procfs is present
AC_CHECK_FILES([/proc/self/stat])

# See if NetBSD pkgsrc is installed
AC_CHECK_FILE([/usr/pkg/lib],[AC_SUBST(LIBDIR,[-L/usr/pkg/lib\ -R/usr/pkg/lib])])
AC_CHECK_FILE([/usr/pkg/include],[AC_SUBST(INCLUDEDIR,[-I/usr/pkg/include])])

AC_CHECK_FUNCS( [wprintf futimes wcwidth wcswidth getopt_long] ) 
AC_CHECK_HEADERS([getopt.h termio.h sys/resource.h])

# Check for RLIMIT_AS in sys/resource.h.
AC_MSG_CHECKING([for RLIMIT_AS in sys/resource.h])
AC_TRY_COMPILE([#include <sys/resource.h>],
[int tmp; tmp=RLIMIT_AS;], have_rlimit_as=yes, have_rlimit_as=no)
if test "$have_rlimit_as" = yes; then
    AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_RLIMIT_AS], [1],
			  [Define to 1 if HAVE_RLIMIT_AS is defined in <sys/resource.h>.])
else
    AC_MSG_RESULT(no)
fi

# Check for RLIMIT_MEMLOCK in sys/resource.h.
AC_MSG_CHECKING([for RLIMIT_MEMLOCK in sys/resource.h])
AC_TRY_COMPILE([#include <sys/resource.h>],
[int tmp; tmp=RLIMIT_MEMLOCK;], have_rlimit_as=yes, have_rlimit_as=no)
if test "$have_rlimit_as" = yes; then
    AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_RLIMIT_MEMLOCK], [1],
			  [Define to 1 if HAVE_RLIMIT_MEMLOCK is defined in <sys/resource.h>.])
else
    AC_MSG_RESULT(no)
fi

# Check for RLIMIT_RSS in sys/resource.h.
AC_MSG_CHECKING([for RLIMIT_RSS in sys/resource.h])
AC_TRY_COMPILE([#include <sys/resource.h>],
[int tmp; tmp=RLIMIT_RSS;], have_rlimit_as=yes, have_rlimit_as=no)
if test "$have_rlimit_as" = yes; then
    AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_RLIMIT_RSS], [1],
			  [Define to 1 if HAVE_RLIMIT_RSS is defined in <sys/resource.h>.])
else
    AC_MSG_RESULT(no)
fi

# Check for RLIMIT_NPROC in sys/resource.h.
AC_MSG_CHECKING([for RLIMIT_NPROC in sys/resource.h])
AC_TRY_COMPILE([#include <sys/resource.h>],
[int tmp; tmp=RLIMIT_NPROC;], have_rlimit_as=yes, have_rlimit_as=no)
if test "$have_rlimit_as" = yes; then
    AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_RLIMIT_NPROC], [1],
			  [Define to 1 if HAVE_RLIMIT_NPROC is defined in <sys/resource.h>.])
else
    AC_MSG_RESULT(no)
fi


AC_CHECK_LIB(socket, connect)
AC_CHECK_LIB(rt, nanosleep)

AC_CHECK_FUNCS(wcsdup wcsndup wcslen wcscasecmp wcsncasecmp)

# Check if we have ncurses, and use it rather than curses if possible.
AC_CHECK_HEADERS([ncurses.h],[AC_SUBST(CURSESLIB,[ncurses]) AC_DEFINE(HAVE_NCURSES_H)],[AC_SUBST(CURSESLIB,[curses])])

#Force use of ncurses if it is avialable via NetBSD pkgsrc. This is an
#ugly kludge to force NetBSD to use ncurses, since NetBSDs own version
#does not properly support terminfo.
AC_CHECK_FILE([/usr/pkg/include/ncurses.h],[AC_SUBST(CURSESLIB,[ncurses]) AC_DEFINE(HAVE_NCURSES_H)])

AC_CONFIG_FILES([Makefile fish.spec doc_src/fish.1 doc_src/Doxyfile init/fish init/fish_interactive.fish init/fish_complete.fish])
AC_OUTPUT
