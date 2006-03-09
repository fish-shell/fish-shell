# Process this file with autoconf to produce a configure script.
AC_INIT(fish,1.21.2,fish-users@lists.sf.net)

# If needed, run autoconf to regenerate the configure file
AC_MSG_CHECKING([if autoconf needs to be run])
if test configure -ot configure.ac; then
	AC_MSG_RESULT([yes])
	if which autoconf >/dev/null; then
		AC_MSG_NOTICE([running autoconf])
		if autoconf; then
			./configure $@
		fi
		exit			
	else
		AC_MSG_ERROR( [cannot find the autoconf program in your path.
This program is needs to be run whenever the configure.ac file is modified.
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
This program is needs to be run whenever the configure.ac file is modified.
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

# Check for doxygen, which is needed to build
AC_CHECK_PROG( has_doxygen, [doxygen], "true")

if ! test $has_doxygen = "true"; then
	AC_MSG_ERROR( [cannot find the Doxygen program in your path.
This program is needed to build fish.
Please install it and try again.])
fi

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

# Test cpu for special handling of ppc
AC_CANONICAL_TARGET

if test $target_cpu = powerpc; then
   AC_DEFINE([TPUTS_KLUDGE],[1],[Evil kludge to get Power based machines to work])
fi

AC_DEFINE_UNQUOTED([CPU],[L"$target_cpu"],[CPU type])


# Set up PREFIX and related preprocessor symbols. Fish needs to know
# where it will be installed. One of the reasons for this is so that
# it can make sure the fish installation directory is in the path
# during startup.
if [[ "$prefix" = NONE ]]; then
	AC_DEFINE_UNQUOTED( [PREFIX], L"/usr/local", [Installation directory])
	AC_SUBST( PREFIX, /usr/local)
	export prefix=/usr/local
else
	AC_DEFINE_UNQUOTED( [PREFIX], L"$prefix", [Installation directory])
	AC_SUBST( PREFIX, [$prefix])
fi
AC_DEFINE_UNQUOTED( SYSCONFDIR, [L"$(eval echo $sysconfdir)"], [System configuration directory] )
AC_DEFINE_UNQUOTED( DATADIR, [L"$(eval echo $datadir)"], [System configuration directory] )
AC_SUBST( SYSCONFDIR, ["$(eval echo $sysconfdir)"] )
AC_SUBST( DATADIR, ["$(eval echo $datadir)"] )


# Set up the directory where the documentation files should be
# installed
AC_ARG_VAR( [docdir], [Documentation direcotry] )

if test -z $docdir; then
   AC_SUBST(docdir,[$datadir/doc/fish])
fi

AC_DEFINE_UNQUOTED( DOCDIR, [L"$(eval echo $docdir)"], [Documentation directory] )


# Set up locale directory. This is where the .po files will be
# installed.
AC_DEFINE_UNQUOTED( [LOCALEDIR], "$(eval echo $datadir)/locale", [Locale directory])
AC_SUBST( [LOCALEDIR], [$datadir/locale])

# See if Linux procfs is present. This is used to get extra
# information about running processes.
AC_CHECK_FILES([/proc/self/stat])

# Check for presense of various libraries
AC_SEARCH_LIBS( gettext, intl, AC_SUBST( HAVE_GETTEXT, [1] ), AC_SUBST( HAVE_GETTEXT, [0] ) )
AC_SEARCH_LIBS( connect, socket, , [AC_MSG_ERROR([Cannot find the socket library, needed to build this package.] )] )
AC_SEARCH_LIBS( nanosleep, rt, , [AC_MSG_ERROR([Cannot find the rt library, needed to build this package.] )] )
AC_SEARCH_LIBS( setupterm, [ncurses curses], , [AC_MSG_ERROR([Could not find a curses implementation, needed to build fish])] )

# Check for presense of various header files
AC_CHECK_HEADERS([getopt.h termio.h sys/resource.h term.h ncurses/term.h libintl.h ncurses.h curses.h])

AC_CHECK_HEADER([regex.h],
				[AC_DEFINE([HAVE_REGEX_H], [1], [Define to 1 if you have the <regex.h> header file.])],
				[AC_MSG_ERROR([Could not find the header regex.h, needed to build fish])])

# Check for presense of various functions
AC_CHECK_FUNCS( wcsdup wcsndup wcslen wcscasecmp wcsncasecmp gettext fwprintf )
AC_CHECK_FUNCS( futimes wcwidth wcswidth getopt_long wcstok fputwc fgetwc )
AC_CHECK_FUNCS( wcstol dcgettext )

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

if test "$ac_cv_func_fwprintf" = yes; then

	# Check if fwprintf is broken
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

AC_DEFINE([HAVE_TRANSLATE_H], [1],
			  [Define to 1 if the wgettext function should be used for translating strings.])

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

# Tell the world what we know
AC_CONFIG_FILES([Makefile fish.spec doc_src/fish.1 doc_src/Doxyfile etc/fish etc/fish_interactive.fish seq])
AC_OUTPUT

echo "fish is now configured."
echo "Now run 'make' and 'make install' to built and install fish."
echo "Good luck!"
