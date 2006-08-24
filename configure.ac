# Process this file with autoconf to produce a configure script.
AC_INIT(fish,1.21.11,fish-users@lists.sf.net)

# If needed, run autoconf to regenerate the configure file
AC_MSG_CHECKING([if autoconf needs to be run])
if test configure -ot configure.ac; then
	AC_MSG_RESULT([yes])
	if which autoconf >/dev/null; then
		# No need to provide any error messages if autoconf fails, the
		# shell and autconf should take care of that themselves
		AC_MSG_NOTICE([running autoconf])
		if autoconf; then
			./configure "$@"
		fi
		exit	
	else
		AC_MSG_ERROR( [cannot find the autoconf program in your path.
This program needs to be run whenever the configure.ac file is modified.
Please install it and try again.])
	fi
else
	AC_MSG_RESULT([no])
fi


# If needed, run autoheader to regenerate config.h.in
AC_MSG_CHECKING([if autoheader needs to be run])
if test ! -f ./config.h.in -o config.h.in -ot configure.ac; then
	AC_MSG_RESULT([yes])
	if which autoheader >/dev/null; then
		AC_MSG_NOTICE([running autoheader])
		autoheader
	else
		AC_MSG_ERROR( [cannot find the autoheader program in your path.
This program needs to be run whenever the configure.ac file is modified.
Please install it and try again.])
	fi
else
	AC_MSG_RESULT([no])
fi


# Detect directories which may contain additional headers, libraries
# and commands. This needs to be done early - before Autoconf starts
# to mess with CFLAGS and all the other environemnt variables.
for i in /usr/pkg /sw /opt /opt/local; do

	AC_MSG_CHECKING([for $i/include include directory])
	if test -d $i/include; then
		AC_MSG_RESULT(yes)
		CPPFLAGS="$CPPFLAGS -I$i/include/"
		CFLAGS="$CFLAGS -I$i/include/"
	else
   	AC_MSG_RESULT(no)
	fi

	AC_MSG_CHECKING([for $i/lib library directory])
	if test -d $i/lib; then
		AC_MSG_RESULT(yes)
		LDFLAGS="$LDFLAGS -L$i/lib/ -R$i/lib/"
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

AC_SUBST( optbindirs, $optbindirs )

# Tell autoconf to create config.h header 
AC_CONFIG_HEADERS(config.h)

# Set up various programs needed for install
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

# Check for seq command. If missing, make sure fallback shellscript
# implementation is installed
AC_CHECK_PROG( SEQ_FALLBACK, seq, [ ], [seq])

# Optionally drop xsel command
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

# These help out with getting better prototypes on some platforms

#
# Test if the compiler accepts the -std=c99 flag. If so, using it
# increases the odds of correct compilation, since we want to use the
# *wprintf functions, which where defined in C99. On some platforms
# (Solaris 10) adding -std=c99 in turn requires that _POSIX_C_SOURCE
# be defined to 200112L otherwise several POSIX-specific, non-ISO-C99 
# types/prototypes are made unavailable e.g. siginfo_t.  Finally,
# defining _XOPEN_SOURCE to 600 is compatible with the
# _POSIX_C_SOURCE value and provides a little assurance that
# extension functions' prototypes are available, e.g. killpg().

XCFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -std=c99 -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L"
AC_MSG_CHECKING(if -std=c99 -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L works)
AC_CACHE_VAL(local_cv_has__std_c99,[
AC_TRY_RUN([
#include <stdlib.h>
#include <stdio.h>

int main() {
return 0;
}],
local_cv_has__std_c99=yes,
local_cv_has__std_c99=no,
)])
AC_MSG_RESULT($local_cv_has__std_c99)
case x$local_cv_has__std_c99 in
xno) CFLAGS="$XCFLAGS" ;;
esac

#
# If we are using gcc, set some flags that increase the odds of the
# compiler producing a working binary...
#

if test "$CC" = gcc; then

	#
	# -fno-optimize-sibling-calls seems to work around a bug where
	# sending a SIGWINCH to fish on NetBSD causes fish to exit when
	# compiled with GCC 3.3.3. This is probably either a compiler bug
	# or a libc bug, but adding this flag seems to fix things for
	# now. Long term, the real problem should be tracked down and
	# truly fixed, at which point we can remove this silly flag. This
	# bug has been verified to not exist on Linux using GCC 3.3.3.
	#

	CFLAGS="$CFLAGS -fno-optimize-sibling-calls"


	#
	# -Wall is there to keep me on my toes
	#

	CFLAGS="$CFLAGS -Wall"

fi

#
# If we are compiling against glibc, set some flags to work around
# some rather stupid attempts to hide prototypes for *wprintf
# functions, as well as prototypes of various gnu extensions.
#

AC_MSG_CHECKING([if we are compiling against glibc])
AC_RUN_IFELSE(
	[AC_LANG_PROGRAM([
#include <stdlib.h>
#ifdef __GLIBC__
#define STATUS 0
#else
#define STATUS 1
#endif],
		[return STATUS;])], 
	[glibc=yes], 
	[glibc=no] )

if test "$glibc" = yes; then
	AC_MSG_RESULT(yes)

	#
	# This gives us access to prototypes for gnu extensions and C99
	# functions if we are compiling agains glibc. All GNU extensions
	# that are used must have a fallback implementation available in
	# fallback.h, in order to keep fish working on non-gnu platforms.
	#

	AC_DEFINE( _GNU_SOURCE, 1, [Macro to enable additional prototypes under Glibc])
	AC_DEFINE( _ISOC99_SOURCE, 1, [Macro to enable additional prototypes under Glibc])
else
	AC_MSG_RESULT(no)
fi

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

# Solaris-specific flags go here
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


# BSD-specific flags go here
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


# Set up PREFIX and related preprocessor symbols. Fish needs to know
# where it will be installed. One of the reasons for this is so that
# it can make sure the fish installation directory is in the path
# during startup.
if [[ "$prefix" = NONE ]]; then
	export prefix=/usr/local
	AC_SUBST( prefix, /usr/local)
else
	AC_SUBST( prefix, [$prefix])
fi


# Set up the directory where the documentation files should be
# installed
AC_ARG_VAR( [docdir], [Documentation direcotry] )

if test -z $docdir; then
   AC_SUBST(docdir, [$datadir/doc/fish] )
else
   AC_SUBST(docdir, [$docdir])
fi

# Set up locale directory. This is where the .po files will be
# installed.
AC_SUBST( [localedir], [$datadir/locale])

# See if Linux procfs is present. This is used to get extra
# information about running processes.
AC_CHECK_FILES([/proc/self/stat])

# This is ued to tell the wgetopt library to translate strings. This
# way wgetopt can be dropped into any project without requiring i18n.

AC_DEFINE([HAVE_TRANSLATE_H], [1],
			  [Define to 1 if the wgettext function should be used for translating strings.])

# Check for presense of various libraries
AC_SEARCH_LIBS( gettext, intl,,)
AC_SEARCH_LIBS( connect, socket, , [AC_MSG_ERROR([Cannot find the socket library, needed to build this package.] )] )
AC_SEARCH_LIBS( nanosleep, rt, , [AC_MSG_ERROR([Cannot find the rt library, needed to build this package.] )] )
AC_SEARCH_LIBS( setupterm, [ncurses curses], , [AC_MSG_ERROR([Could not find a curses implementation, needed to build fish])] )

# Check for presense of various header files
AC_CHECK_HEADERS([getopt.h termio.h sys/resource.h term.h ncurses/term.h libintl.h ncurses.h curses.h stropts.h siginfo.h sys/select.h sys/ioctl.h sys/termios.h])

AC_CHECK_HEADER([regex.h],
				[AC_DEFINE([HAVE_REGEX_H], [1], [Define to 1 if you have the <regex.h> header file.])],
				[AC_MSG_ERROR([Could not find the header regex.h, needed to build fish])])

# Check for presense of various functions
AC_CHECK_FUNCS( gettext wcsdup wcsndup wcslen wcscasecmp wcsncasecmp fwprintf )
AC_CHECK_FUNCS( futimes wcwidth wcswidth getopt_long wcstok fputwc fgetwc )
AC_CHECK_FUNCS( wcstol dcgettext wcslcat wcslcpy lrand48_r killpg)

# The Makefile also needs to know if we have gettext, so it knows if the translations should be installed.
AC_CHECK_FUNC( gettext, AC_SUBST( HAVE_GETTEXT, 1 ), AC_SUBST( HAVE_GETTEXT, 0 ) )

# Here follows a list of small programs used to test for various
# features that Autoconf doesn't tell us about

# Check if realpath accepts null for its second argument
AC_MSG_CHECKING([if realpath accepts null for its second argument])
AC_RUN_IFELSE(
	[AC_LANG_PROGRAM([
#include <limits.h>
#include <errno.h>
#include <stdlib.h>],
		[int status; char *res; res = realpath( "somefile", 0 ); status = !(res != 0 || errno == ENOENT); exit( status );])], 
	[have_realpath_null=yes], 
	[have_realpath_null=no] )

if test "$have_realpath_null" = yes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_REALPATH_NULL], [1],
		[Define to 1 if realpath accepts null for its second argument.])
else
	AC_MSG_RESULT(no)
fi


# Check if struct winsize exists
AC_MSG_CHECKING([if struct winsize and TIOCGWINSZ exist])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

],
	[struct winsize termsize = {0}; TIOCGWINSZ;])], 
	[AC_MSG_RESULT(yes); AC_DEFINE([HAVE_WINSIZE], [1], [Define to 1 if the winsize struct and TIOCGWINSZ macro exist])], 
	AC_MSG_RESULT(no))

# If we have a fwprintf in libc, test that it actually works. As of
# March 2006, it is broken under Dragonfly BSD.
if test "$ac_cv_func_fwprintf" = yes; then

	AC_MSG_CHECKING([if fwprintf is broken])
	AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <wchar.h>
			],
			[
				setlocale( LC_ALL, "" );
				fwprintf( stderr, L"%ls%ls", L"", L"fish:" );
			])],
	    [
			AC_MSG_RESULT(no)
		],
	    [
			AC_MSG_RESULT([yes])
			AC_DEFINE([HAVE_BROKEN_FWPRINTF], [1], [Define to 1 one if the implemented fwprintf is broken])
		] )

fi


# Check for _nl_msg_cat_cntr symbol
AC_MSG_CHECKING([for _nl_msg_cat_cntr symbol])
AC_TRY_LINK([#if HAVE_LIBINTL_H]
[#include <libintl.h>]
[#endif],
[extern int  _nl_msg_cat_cntr;]
[int tmp = _nl_msg_cat_cntr; exit(tmp);], have__nl_msg_cat_cntr=yes, have__nl_msg_cat_cntr=no)
if test "$have__nl_msg_cat_cntr" = yes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE__NL_MSG_CAT_CNTR], [1],
	          [Define to 1 if the _nl_msg_cat_cntr symbol is exported.])	
else
    AC_MSG_RESULT(no)
fi

# Check if del_curterm is broken - in that case we redefine
# del_curterm as a no-op, to avoid a double-free

AC_MSG_CHECKING([If del_curterm is broken])
case $target_os in
	*bsd*)
		AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_BROKEN_DEL_CURTERM],[1],[del_curterm si broken, redefine it to a no-op to avoid a double-free bug])
		;;
	*)
		AC_MSG_RESULT(no)
		;;
esac

# Tell the world what we know
AC_CONFIG_FILES([Makefile fish.spec doc_src/fish.1 doc_src/Doxyfile seq])
AC_OUTPUT

echo "fish is now configured."
echo "Use 'make' and 'make install' to build and install fish."
