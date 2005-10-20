# Process this file with autoconf to produce a configure script.
AC_INIT(fish,1.16.0,axel@liljencrantz.se)

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
   	AC_SUBST(sysconfdir,[/etc])
	export prefix=/usr/local
else
	AC_DEFINE_UNQUOTED( [PREFIX], L"$prefix", [Installation directory])
	AC_SUBST( PREFIX, [$prefix])
   	AC_SUBST(sysconfdir,[/etc])
fi

if echo $prefix | grep \^$HOME >/dev/null; then
  	AC_SUBST(sysconfdir,[$HOME/etc])
	AC_MSG_NOTICE(["Install in $HOME"])
fi
AC_SUBST(fishdir,[/fish.d])
AC_SUBST(fishfile,[/fish])
AC_SUBST(fishinputfile,[/fish_inputrc])

AC_ARG_VAR( [docdir], [Documentation direcotry] )

if test -z $docdir; then
   AC_SUBST(docdir,[$datadir/doc/fish])
fi

AC_DEFINE_UNQUOTED( DOCDIR, [L"$(eval echo $docdir)"], [Documentation directory] )
AC_DEFINE_UNQUOTED( SYSCONFDIR, [L"$sysconfdir"], [System configuration directory] )

# See if Linux procfs is present
AC_CHECK_FILES([/proc/self/stat])

# See if NetBSD pkgsrc is installed
AC_CHECK_FILE([/usr/pkg/lib],[AC_SUBST(LIBDIR,[-L/usr/pkg/lib\ -R/usr/pkg/lib])])
AC_CHECK_FILE([/usr/pkg/include],[AC_SUBST(INCLUDEDIR,[-I/usr/pkg/include])])

AC_CHECK_FUNCS( [wprintf futimes wcwidth wcswidth] ) 
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

AC_CHECK_LIB(socket, connect)
AC_CHECK_LIB(rt, nanosleep)

# Check if we have ncurses, and use it rather than curses if possible.
AC_CHECK_HEADERS([ncurses.h],[AC_SUBST(CURSESLIB,[ncurses]) AC_DEFINE(HAVE_NCURSES_H)],[AC_SUBST(CURSESLIB,[curses])])

#Force use of ncurses if it is avialable via NetBSD pkgsrc. This is an
#ugly kludge to force NetBSD to use ncurses, since NetBSDs own version
#does not properly support terminfo.
AC_CHECK_FILE([/usr/pkg/include/ncurses.h],[AC_SUBST(CURSESLIB,[ncurses]) AC_DEFINE(HAVE_NCURSES_H)])

AC_CONFIG_FILES([Makefile fish.spec doc_src/fish.1 doc_src/Doxyfile init/fish])
AC_OUTPUT
