# Process this file with autoconf to produce a configure script.
AC_INIT(fish,1.20.2,fish-users@lists.sf.net)

for i in /usr/pkg /sw /opt /opt/local; do

	AC_MSG_CHECKING([for $i/include include directory])
	if test -d $i/include; then
		AC_MSG_RESULT(yes)
		CPPFLAGS="$CPPFLAGS -I$i/include/"
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

# If needed, run autoheader automatically
AC_MSG_CHECKING([if autoheader needs to be run])
if test ! -f ./config.h.in -o config.h.in -ot configure.ac; then
	AC_MSG_RESULT([yes])
	if which autoheader >/dev/null; then
		AC_MSG_NOTICE([running autoheader])
		autoheader
	else
		AC_MSG_ERROR( [cannot find the autoheader program in your path.
This program is needed because the configure.ac file has been modified.
Please install it and try again.])
	fi
else
	AC_MSG_RESULT([no])
fi

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

# Check for seq program. If missing, install fallback shellscript implementation
AC_CHECK_PROG( SEQ_FALLBACK, seq, [ ], [seq])

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

# Test cpu for special handling of ppc
AC_CANONICAL_TARGET

if test $target_cpu = powerpc; then
   AC_DEFINE([TPUTS_KLUDGE],[1],[Evil kludge to get Power based machines to work])
fi

AC_DEFINE_UNQUOTED([CPU],[L"$target_cpu"],[CPU type])


# Set up installation directories
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

# Set up locale directory
AC_DEFINE_UNQUOTED( [LOCALEDIR], "$(eval echo $datadir)/locale", [Locale directory])
AC_SUBST( [LOCALEDIR], [$datadir/locale])

# See if Linux procfs is present
AC_CHECK_FILES([/proc/self/stat])

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

# Check if realpath accepts null for its second argument
AC_MSG_CHECKING([if realpath accepts null for its second argument])
AC_RUN_IFELSE(
	[AC_LANG_PROGRAM([#include <limits.h>
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


# Check for libraries
AC_CHECK_LIB(socket, connect)
AC_CHECK_LIB(rt, nanosleep)
AC_CHECK_LIB(intl, gettext)

# Check for various header files
AC_CHECK_HEADERS([getopt.h termio.h sys/resource.h term.h ncurses/term.h libintl.h])

AC_CHECK_HEADER([regex.h],
				[AC_DEFINE([HAVE_REGEX_H], [1], [Define to 1 if you have the <regex.h> header file.])],
				[AC_MSG_ERROR([Could not find the header regex.h, needed to build fish])])


# Check for various functions, and insert results into config.h
AC_CHECK_FUNCS( wcsdup wcsndup wcslen wcscasecmp wcsncasecmp gettext fwprintf )
AC_CHECK_FUNCS( futimes wcwidth wcswidth getopt_long wcstok fputwc fgetwc )
AC_CHECK_FUNCS( wcstol dcgettext )

AC_DEFINE([HAVE_TRANSLATE_H], [1],
			  [Define to 1 if the wgettext function should be used for translating strings.])

# Check again for gettext library, and insert results into the Makefile
AC_CHECK_FUNC(gettext, AC_SUBST(HAVE_GETTEXT,1), AC_SUBST(HAVE_GETTEXT,0) )

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

# Check if we have ncurses, and use it rather than curses if possible.
AC_SEARCH_LIBS( setupterm, [ncurses curses], [ AC_MSG_NOTICE([Found curses implementation])], [AC_MSG_ERROR([Could not find a curses implementation, needed to build fish])] )

AC_CONFIG_FILES([Makefile fish.spec doc_src/fish.1 doc_src/Doxyfile init/fish init/fish_interactive.fish init/fish_complete.fish seq])
AC_OUTPUT

echo "Now run 'make' and 'make install' to built and install fish."
echo "Good luck!"
