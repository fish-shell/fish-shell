dnl Process this file with autoconf to produce a configure script.

dnl NOTE FOR MAINTAINERS: Do not use minor version numbers 08 or 09 because
dnl the leading zeros may cause them to be treated as invalid octal constants
dnl if a PCRE2 user writes code that uses PCRE2_MINOR as a number. There is now
dnl a check further down that throws an error if 08 or 09 are used.

dnl The PCRE2_PRERELEASE feature is for identifying release candidates. It might
dnl be defined as -RC2, for example. For real releases, it should be empty.

m4_define(pcre2_major, [10])
m4_define(pcre2_minor, [32])
m4_define(pcre2_prerelease, [])
m4_define(pcre2_date, [2018-09-10])

# NOTE: The CMakeLists.txt file searches for the above variables in the first
# 50 lines of this file. Please update that if the variables above are moved.

# Libtool shared library interface versions (current:revision:age)
m4_define(libpcre2_8_version,     [7:1:7])
m4_define(libpcre2_16_version,    [7:1:7])
m4_define(libpcre2_32_version,    [7:1:7])
m4_define(libpcre2_posix_version, [2:1:0])

AC_PREREQ(2.57)
AC_INIT(PCRE2, pcre2_major.pcre2_minor[]pcre2_prerelease, , pcre2)
AC_CONFIG_SRCDIR([src/pcre2.h.in])
AM_INIT_AUTOMAKE([dist-bzip2 dist-zip])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_HEADERS(src/config.h)

# This was added at the suggestion of libtoolize (03-Jan-10)
AC_CONFIG_MACRO_DIR([m4])

# The default CFLAGS in Autoconf are "-g -O2" for gcc and just "-g" for any
# other compiler. There doesn't seem to be a standard way of getting rid of the
# -g (which I don't think is needed for a production library). This fudge seems
# to achieve the necessary. First, we remember the externally set values of
# CFLAGS. Then call the AC_PROG_CC macro to find the compiler - if CFLAGS is
# not set, it will be set to Autoconf's defaults. Afterwards, if the original
# values were not set, remove the -g from the Autoconf defaults.

remember_set_CFLAGS="$CFLAGS"

AC_PROG_CC
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS

if test "x$remember_set_CFLAGS" = "x"
then
  if test "$CFLAGS" = "-g -O2"
  then
    CFLAGS="-O2"
  elif test "$CFLAGS" = "-g"
  then
    CFLAGS=""
  fi
fi

# FISH PATCH
# Enable maintainer mode to avoid spurious rebuilds due to timestamps in git
# not being stored. Discussion in https://github.com/fish-shell/fish-shell/issues/2469
AM_MAINTAINER_MODE
# END FISH PATCH

# This is a new thing required to stop a warning from automake 1.12
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Check for a 64-bit integer type
AC_TYPE_INT64_T

AC_PROG_INSTALL
AC_LIBTOOL_WIN32_DLL
LT_INIT
AC_PROG_LN_S

# Check for GCC visibility feature

PCRE2_VISIBILITY

# Versioning

PCRE2_MAJOR="pcre2_major"
PCRE2_MINOR="pcre2_minor"
PCRE2_PRERELEASE="pcre2_prerelease"
PCRE2_DATE="pcre2_date"

if test "$PCRE2_MINOR" = "08" -o "$PCRE2_MINOR" = "09"
then
  echo "***"
  echo "*** Minor version number $PCRE2_MINOR must not be used. ***"
  echo "*** Use only 00 to 07 or 10 onwards, to avoid octal issues. ***"
  echo "***"
  exit 1
fi

AC_SUBST(PCRE2_MAJOR)
AC_SUBST(PCRE2_MINOR)
AC_SUBST(PCRE2_PRERELEASE)
AC_SUBST(PCRE2_DATE)

# Set a more sensible default value for $(htmldir).
if test "x$htmldir" = 'x${docdir}'
then
  htmldir='${docdir}/html'
fi

# Force an error for PCRE1 size options
AC_ARG_ENABLE(pcre8,,,enable_pcre8=no)
AC_ARG_ENABLE(pcre16,,,enable_pcre16=no)
AC_ARG_ENABLE(pcre32,,,enable_pcre32=no)

if test "$enable_pcre8$enable_pcre16$enable_pcre32" != "nonono"
then
  echo "** ERROR: Use --[[en|dis]]able-pcre2-[[8|16|32]], not --[[en|dis]]able-pcre[[8|16|32]]"
  exit 1
fi

# Handle --disable-pcre2-8 (enabled by default)
AC_ARG_ENABLE(pcre2-8,
              AS_HELP_STRING([--disable-pcre2-8],
                             [disable 8 bit character support]),
              , enable_pcre2_8=unset)
AC_SUBST(enable_pcre2_8)

# Handle --enable-pcre2-16 (disabled by default)
AC_ARG_ENABLE(pcre2-16,
              AS_HELP_STRING([--enable-pcre2-16],
                             [enable 16 bit character support]),
              , enable_pcre2_16=unset)
AC_SUBST(enable_pcre2_16)

# Handle --enable-pcre2-32 (disabled by default)
AC_ARG_ENABLE(pcre2-32,
              AS_HELP_STRING([--enable-pcre2-32],
                             [enable 32 bit character support]),
              , enable_pcre2_32=unset)
AC_SUBST(enable_pcre2_32)

# Handle --dnable-debug (disabled by default)
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug],
                             [enable debugging code]),
              , enable_debug=no)

# Handle --enable-jit (disabled by default)
AC_ARG_ENABLE(jit,
              AS_HELP_STRING([--enable-jit],
                             [enable Just-In-Time compiling support]),
              , enable_jit=no)

# This code enables JIT if the hardware supports it.
if test "$enable_jit" = "auto"; then
  AC_LANG(C)
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
  #define SLJIT_CONFIG_AUTO 1
  #include "src/sljit/sljitConfigInternal.h"
  #if (defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)
  #error unsupported
  #endif]])], enable_jit=yes, enable_jit=no)
  echo checking for JIT support on this hardware... $enable_jit
fi

# Handle --enable-jit-sealloc (disabled by default)
AC_ARG_ENABLE(jit-sealloc,
              AS_HELP_STRING([--enable-jit-sealloc],
                             [enable SELinux compatible execmem allocator in JIT]),
              , enable_jit_sealloc=no)

# Handle --disable-pcre2grep-jit (enabled by default)
AC_ARG_ENABLE(pcre2grep-jit,
              AS_HELP_STRING([--disable-pcre2grep-jit],
                             [disable JIT support in pcre2grep]),
              , enable_pcre2grep_jit=yes)

# Handle --disable-pcre2grep-callout (enabled by default)
AC_ARG_ENABLE(pcre2grep-callout,
              AS_HELP_STRING([--disable-pcre2grep-callout],
                             [disable callout script support in pcre2grep]),
              , enable_pcre2grep_callout=yes)

# Handle --enable-rebuild-chartables
AC_ARG_ENABLE(rebuild-chartables,
              AS_HELP_STRING([--enable-rebuild-chartables],
                             [rebuild character tables in current locale]),
              , enable_rebuild_chartables=no)

# Handle --disable-unicode (enabled by default)
AC_ARG_ENABLE(unicode,
              AS_HELP_STRING([--disable-unicode],
                             [disable Unicode support]),
              , enable_unicode=unset)

# Handle newline options
ac_pcre2_newline=lf
AC_ARG_ENABLE(newline-is-cr,
              AS_HELP_STRING([--enable-newline-is-cr],
                             [use CR as newline character]),
              ac_pcre2_newline=cr)
AC_ARG_ENABLE(newline-is-lf,
              AS_HELP_STRING([--enable-newline-is-lf],
                             [use LF as newline character (default)]),
              ac_pcre2_newline=lf)
AC_ARG_ENABLE(newline-is-crlf,
              AS_HELP_STRING([--enable-newline-is-crlf],
                             [use CRLF as newline sequence]),
              ac_pcre2_newline=crlf)
AC_ARG_ENABLE(newline-is-anycrlf,
              AS_HELP_STRING([--enable-newline-is-anycrlf],
                             [use CR, LF, or CRLF as newline sequence]),
              ac_pcre2_newline=anycrlf)
AC_ARG_ENABLE(newline-is-any,
              AS_HELP_STRING([--enable-newline-is-any],
                             [use any valid Unicode newline sequence]),
              ac_pcre2_newline=any)
AC_ARG_ENABLE(newline-is-nul,
              AS_HELP_STRING([--enable-newline-is-nul],
                             [use NUL (binary zero) as newline character]),
              ac_pcre2_newline=nul)
enable_newline="$ac_pcre2_newline"

# Handle --enable-bsr-anycrlf
AC_ARG_ENABLE(bsr-anycrlf,
              AS_HELP_STRING([--enable-bsr-anycrlf],
                             [\R matches only CR, LF, CRLF by default]),
              , enable_bsr_anycrlf=no)

# Handle --enable-never-backslash-C
AC_ARG_ENABLE(never-backslash-C,
              AS_HELP_STRING([--enable-never-backslash-C],
                             [use of \C causes an error]),
              , enable_never_backslash_C=no)

# Handle --enable-ebcdic
AC_ARG_ENABLE(ebcdic,
              AS_HELP_STRING([--enable-ebcdic],
                             [assume EBCDIC coding rather than ASCII; incompatible with --enable-utf; use only in (uncommon) EBCDIC environments; it implies --enable-rebuild-chartables]),
              , enable_ebcdic=no)

# Handle --enable-ebcdic-nl25
AC_ARG_ENABLE(ebcdic-nl25,
              AS_HELP_STRING([--enable-ebcdic-nl25],
                             [set EBCDIC code for NL to 0x25 instead of 0x15; it implies --enable-ebcdic]),
              , enable_ebcdic_nl25=no)

# Handle --enable-pcre2grep-libz
AC_ARG_ENABLE(pcre2grep-libz,
              AS_HELP_STRING([--enable-pcre2grep-libz],
                             [link pcre2grep with libz to handle .gz files]),
              , enable_pcre2grep_libz=no)

# Handle --enable-pcre2grep-libbz2
AC_ARG_ENABLE(pcre2grep-libbz2,
              AS_HELP_STRING([--enable-pcre2grep-libbz2],
                             [link pcre2grep with libbz2 to handle .bz2 files]),
              , enable_pcre2grep_libbz2=no)

# Handle --with-pcre2grep-bufsize=N
AC_ARG_WITH(pcre2grep-bufsize,
              AS_HELP_STRING([--with-pcre2grep-bufsize=N],
                             [pcre2grep initial buffer size (default=20480, minimum=8192)]),
              , with_pcre2grep_bufsize=20480)

# Handle --with-pcre2grep-max-bufsize=N
AC_ARG_WITH(pcre2grep-max-bufsize,
              AS_HELP_STRING([--with-pcre2grep-max-bufsize=N],
                             [pcre2grep maximum buffer size (default=1048576, minimum=8192)]),
              , with_pcre2grep_max_bufsize=1048576)

# Handle --enable-pcre2test-libedit
AC_ARG_ENABLE(pcre2test-libedit,
              AS_HELP_STRING([--enable-pcre2test-libedit],
                             [link pcre2test with libedit]),
              , enable_pcre2test_libedit=no)

# Handle --enable-pcre2test-libreadline
AC_ARG_ENABLE(pcre2test-libreadline,
              AS_HELP_STRING([--enable-pcre2test-libreadline],
                             [link pcre2test with libreadline]),
              , enable_pcre2test_libreadline=no)

# Handle --with-link-size=N
AC_ARG_WITH(link-size,
            AS_HELP_STRING([--with-link-size=N],
                           [internal link size (2, 3, or 4 allowed; default=2)]),
            , with_link_size=2)

# Handle --with-parens-nest-limit=N
AC_ARG_WITH(parens-nest-limit,
            AS_HELP_STRING([--with-parens-nest-limit=N],
                           [nested parentheses limit (default=250)]),
            , with_parens_nest_limit=250)

# Handle --with-heap-limit
AC_ARG_WITH(heap-limit,
            AS_HELP_STRING([--with-heap-limit=N],
                           [default limit on heap memory (kibibytes, default=20000000)]),
            , with_heap_limit=20000000)

# Handle --with-match-limit=N
AC_ARG_WITH(match-limit,
            AS_HELP_STRING([--with-match-limit=N],
                           [default limit on internal looping (default=10000000)]),
            , with_match_limit=10000000)

# Handle --with-match-limit-depth=N
# Recognize old synonym --with-match-limit-recursion
#
# Note: In config.h, the default is to define MATCH_LIMIT_DEPTH symbolically as
# MATCH_LIMIT, which in turn is defined to be some numeric value (e.g.
# 10000000). MATCH_LIMIT_DEPTH can otherwise be set to some different numeric
# value (or even the same numeric value as MATCH_LIMIT, though no longer
# defined in terms of the latter).
#
AC_ARG_WITH(match-limit-depth,
            AS_HELP_STRING([--with-match-limit-depth=N],
                           [default limit on match tree depth (default=MATCH_LIMIT)]),
            , with_match_limit_depth=MATCH_LIMIT)

AC_ARG_WITH(match-limit-recursion,,
            , with_match_limit_recursion=UNSET)

# Handle --enable-valgrind
AC_ARG_ENABLE(valgrind,
              AS_HELP_STRING([--enable-valgrind],
                             [enable valgrind support]),
              , enable_valgrind=no)

# Enable code coverage reports using gcov
AC_ARG_ENABLE(coverage,
              AS_HELP_STRING([--enable-coverage],
                             [enable code coverage reports using gcov]),
              , enable_coverage=no)

# Handle --enable-fuzz-support
AC_ARG_ENABLE(fuzz_support,
              AS_HELP_STRING([--enable-fuzz-support],
                             [enable fuzzer support]),
              , enable_fuzz_support=no)

# Handle --disable-stack-for-recursion
# This option became obsolete at release 10.30.
AC_ARG_ENABLE(stack-for-recursion,,
              , enable_stack_for_recursion=yes)

# Original code
# AC_ARG_ENABLE(stack-for-recursion,
#               AS_HELP_STRING([--disable-stack-for-recursion],
#                              [don't use stack recursion when matching]),
#               , enable_stack_for_recursion=yes)

# Set the default value for pcre2-8
if test "x$enable_pcre2_8" = "xunset"
then
  enable_pcre2_8=yes
fi

# Set the default value for pcre2-16
if test "x$enable_pcre2_16" = "xunset"
then
  enable_pcre2_16=no
fi

# Set the default value for pcre2-32
if test "x$enable_pcre2_32" = "xunset"
then
  enable_pcre2_32=no
fi

# Make sure at least one library is selected
if test "x$enable_pcre2_8$enable_pcre2_16$enable_pcre2_32" = "xnonono"
then
  AC_MSG_ERROR([At least one of the 8, 16 or 32 bit libraries must be enabled])
fi

# Unicode is enabled by default.
if test "x$enable_unicode" = "xunset"
then
  enable_unicode=yes
fi

# Convert the newline identifier into the appropriate integer value. These must
# agree with the PCRE2_NEWLINE_xxx values in pcre2.h.

case "$enable_newline" in
  cr)      ac_pcre2_newline_value=1 ;;
  lf)      ac_pcre2_newline_value=2 ;;
  crlf)    ac_pcre2_newline_value=3 ;;
  any)     ac_pcre2_newline_value=4 ;;
  anycrlf) ac_pcre2_newline_value=5 ;;
  nul)     ac_pcre2_newline_value=6 ;;
  *)
  AC_MSG_ERROR([invalid argument \"$enable_newline\" to --enable-newline option])
  ;;
esac

# --enable-ebcdic-nl25 implies --enable-ebcdic
if test "x$enable_ebcdic_nl25" = "xyes"; then
  enable_ebcdic=yes
fi

# Make sure that if enable_ebcdic is set, rebuild_chartables is also enabled.
# Also check that UTF support is not requested, because PCRE2 cannot handle
# EBCDIC and UTF in the same build. To do so it would need to use different
# character constants depending on the mode. Also, EBCDIC cannot be used with
# 16-bit and 32-bit libraries.
#
if test "x$enable_ebcdic" = "xyes"; then
  enable_rebuild_chartables=yes
  if test "x$enable_unicode" = "xyes"; then
    AC_MSG_ERROR([support for EBCDIC and Unicode cannot be enabled at the same time])
  fi
  if test "x$enable_pcre2_16" = "xyes" -o "x$enable_pcre2_32" = "xyes"; then
    AC_MSG_ERROR([EBCDIC support is available only for the 8-bit library])
  fi
fi

# Check argument to --with-link-size
case "$with_link_size" in
  2|3|4) ;;
  *)
  AC_MSG_ERROR([invalid argument \"$with_link_size\" to --with-link-size option])
  ;;
esac

AH_TOP([
/* PCRE2 is written in Standard C, but there are a few non-standard things it
can cope with, allowing it to run on SunOS4 and other "close to standard"
systems.

In environments that support the GNU autotools, config.h.in is converted into
config.h by the "configure" script. In environments that use CMake,
config-cmake.in is converted into config.h. If you are going to build PCRE2 "by
hand" without using "configure" or CMake, you should copy the distributed
config.h.generic to config.h, and edit the macro definitions to be the way you
need them. You must then add -DHAVE_CONFIG_H to all of your compile commands,
so that config.h is included at the start of every source.

Alternatively, you can avoid editing by using -D on the compiler command line
to set the macro values. In this case, you do not have to set -DHAVE_CONFIG_H,
but if you do, default values will be taken from config.h for non-boolean
macros that are not defined on the command line.

Boolean macros such as HAVE_STDLIB_H and SUPPORT_PCRE2_8 should either be
defined (conventionally to 1) for TRUE, and not defined at all for FALSE. All
such macros are listed as a commented #undef in config.h.generic. Macros such
as MATCH_LIMIT, whose actual value is relevant, have defaults defined, but are
surrounded by #ifndef/#endif lines so that the value can be overridden by -D.

PCRE2 uses memmove() if HAVE_MEMMOVE is defined; otherwise it uses bcopy() if
HAVE_BCOPY is defined. If your system has neither bcopy() nor memmove(), make
sure both macros are undefined; an emulation function will then be used. */])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(limits.h sys/types.h sys/stat.h dirent.h)
AC_CHECK_HEADERS([windows.h], [HAVE_WINDOWS_H=1])
AC_CHECK_HEADERS([sys/wait.h], [HAVE_SYS_WAIT_H=1])

AC_CHECK_HEADERS([stdint.h], [PCRE2_HAVE_STDINT_H=1], [PCRE2_HAVE_STDINT_H=0])
AC_CHECK_HEADERS([inttypes.h], [PCRE2_HAVE_INTTYPES_H=1], [PCRE2_HAVE_INTTYPES_H=0])
AC_SUBST([PCRE2_HAVE_STDINT_H])
AC_SUBST([PCRE2_HAVE_INTTYPES_H])

# Conditional compilation
AM_CONDITIONAL(WITH_PCRE2_8, test "x$enable_pcre2_8" = "xyes")
AM_CONDITIONAL(WITH_PCRE2_16, test "x$enable_pcre2_16" = "xyes")
AM_CONDITIONAL(WITH_PCRE2_32, test "x$enable_pcre2_32" = "xyes")
AM_CONDITIONAL(WITH_DEBUG, test "x$enable_debug" = "xyes")
AM_CONDITIONAL(WITH_REBUILD_CHARTABLES, test "x$enable_rebuild_chartables" = "xyes")
AM_CONDITIONAL(WITH_JIT, test "x$enable_jit" = "xyes")
AM_CONDITIONAL(WITH_UNICODE, test "x$enable_unicode" = "xyes")
AM_CONDITIONAL(WITH_VALGRIND, test "x$enable_valgrind" = "xyes")
AM_CONDITIONAL(WITH_FUZZ_SUPPORT, test "x$enable_fuzz_support" = "xyes")

if test "$enable_fuzz_support" = "yes" -a "$enable_pcre2_8" = "no"; then
  echo "** ERROR: Fuzzer support requires the 8-bit library"
  exit 1
fi

# Checks for typedefs, structures, and compiler characteristics.

AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.

AC_CHECK_FUNCS(bcopy memmove strerror mkostemp secure_getenv)

# Check for the availability of libz (aka zlib)

AC_CHECK_HEADERS([zlib.h], [HAVE_ZLIB_H=1])
AC_CHECK_LIB([z], [gzopen], [HAVE_LIBZ=1])

# Check for the availability of libbz2. Originally we just used AC_CHECK_LIB,
# as for libz. However, this had the following problem, diagnosed and fixed by
# a user:
#
#   - libbz2 uses the Pascal calling convention (WINAPI) for the functions
#     under Win32.
#   - The standard autoconf AC_CHECK_LIB fails to include "bzlib.h",
#     therefore missing the function definition.
#   - The compiler thus generates a "C" signature for the test function.
#   - The linker fails to find the "C" function.
#   - PCRE2 fails to configure if asked to do so against libbz2.
#
# Solution:
#
#   - Replace the AC_CHECK_LIB test with a custom test.

AC_CHECK_HEADERS([bzlib.h], [HAVE_BZLIB_H=1])
# Original test
# AC_CHECK_LIB([bz2], [BZ2_bzopen], [HAVE_LIBBZ2=1])
#
# Custom test follows

AC_MSG_CHECKING([for libbz2])
OLD_LIBS="$LIBS"
LIBS="$LIBS -lbz2"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif]],
[[return (int)BZ2_bzopen("conftest", "rb");]])],
[AC_MSG_RESULT([yes]);HAVE_LIBBZ2=1; break;],
AC_MSG_RESULT([no]))
LIBS="$OLD_LIBS"

# Check for the availabiity of libreadline

if test "$enable_pcre2test_libreadline" = "yes"; then
 AC_CHECK_HEADERS([readline/readline.h], [HAVE_READLINE_H=1])
 AC_CHECK_HEADERS([readline/history.h], [HAVE_HISTORY_H=1])
 AC_CHECK_LIB([readline], [readline], [LIBREADLINE="-lreadline"],
   [unset ac_cv_lib_readline_readline;
    AC_CHECK_LIB([readline], [readline], [LIBREADLINE="-ltinfo"],
     [unset ac_cv_lib_readline_readline;
      AC_CHECK_LIB([readline], [readline], [LIBREADLINE="-lcurses"],
       [unset ac_cv_lib_readline_readline;
        AC_CHECK_LIB([readline], [readline], [LIBREADLINE="-lncurses"],
         [unset ac_cv_lib_readline_readline;
	  AC_CHECK_LIB([readline], [readline], [LIBREADLINE="-lncursesw"],
           [unset ac_cv_lib_readline_readline;
	    AC_CHECK_LIB([readline], [readline], [LIBREADLINE="-ltermcap"],
             [LIBREADLINE=""],
             [-ltermcap])],
           [-lncursesw])],
         [-lncurses])],
       [-lcurses])],
     [-ltinfo])])
 AC_SUBST(LIBREADLINE)
 if test -n "$LIBREADLINE"; then
   if test "$LIBREADLINE" != "-lreadline"; then
     echo "-lreadline needs $LIBREADLINE"
     LIBREADLINE="-lreadline $LIBREADLINE"
   fi
 fi
fi


# Check for the availability of libedit. Different distributions put its
# headers in different places. Try to cover the most common ones.

if test "$enable_pcre2test_libedit" = "yes"; then
  AC_CHECK_HEADERS([editline/readline.h], [HAVE_EDITLINE_READLINE_H=1],
    [AC_CHECK_HEADERS([edit/readline/readline.h], [HAVE_READLINE_READLINE_H=1],
      [AC_CHECK_HEADERS([readline/readline.h], [HAVE_READLINE_READLINE_H=1])])])
  AC_CHECK_LIB([edit], [readline], [LIBEDIT="-ledit"])
fi

PCRE2_STATIC_CFLAG=""
if test "x$enable_shared" = "xno" ; then
  AC_DEFINE([PCRE2_STATIC], [1], [
    Define to any value if linking statically (TODO: make nice with Libtool)])
  PCRE2_STATIC_CFLAG="-DPCRE2_STATIC"
fi
AC_SUBST(PCRE2_STATIC_CFLAG)

# Here is where PCRE2-specific defines are handled

if test "$enable_pcre2_8" = "yes"; then
  AC_DEFINE([SUPPORT_PCRE2_8], [], [
    Define to any value to enable the 8 bit PCRE2 library.])
fi

if test "$enable_pcre2_16" = "yes"; then
  AC_DEFINE([SUPPORT_PCRE2_16], [], [
    Define to any value to enable the 16 bit PCRE2 library.])
fi

if test "$enable_pcre2_32" = "yes"; then
  AC_DEFINE([SUPPORT_PCRE2_32], [], [
    Define to any value to enable the 32 bit PCRE2 library.])
fi

if test "$enable_debug" = "yes"; then
  AC_DEFINE([PCRE2_DEBUG], [], [
    Define to any value to include debugging code.])
fi

# Unless running under Windows, JIT support requires pthreads.

if test "$enable_jit" = "yes"; then
  if test "$HAVE_WINDOWS_H" != "1"; then
    AX_PTHREAD([], [AC_MSG_ERROR([JIT support requires pthreads])])
    CC="$PTHREAD_CC"
    CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
    LIBS="$PTHREAD_LIBS $LIBS"
  fi
  AC_DEFINE([SUPPORT_JIT], [], [
    Define to any value to enable support for Just-In-Time compiling.])
else
  enable_pcre2grep_jit="no"
fi

if test "$enable_jit_sealloc" = "yes"; then
  AC_DEFINE([SLJIT_PROT_EXECUTABLE_ALLOCATOR], [1], [
    Define to any non-zero number to enable support for SELinux
    compatible executable memory allocator in JIT. Note that this
    will have no effect unless SUPPORT_JIT is also defined.])
fi

if test "$enable_pcre2grep_jit" = "yes"; then
  AC_DEFINE([SUPPORT_PCRE2GREP_JIT], [], [
    Define to any value to enable JIT support in pcre2grep. Note that this will
    have no effect unless SUPPORT_JIT is also defined.])
fi

if test "$enable_pcre2grep_callout" = "yes"; then
  if test "$HAVE_WINDOWS_H" != "1"; then
    if test "$HAVE_SYS_WAIT_H" != "1"; then
      AC_MSG_ERROR([Callout script support needs sys/wait.h.])
    fi
  fi
  AC_DEFINE([SUPPORT_PCRE2GREP_CALLOUT], [], [
    Define to any value to enable callout script support in pcre2grep.])
fi

if test "$enable_unicode" = "yes"; then
  AC_DEFINE([SUPPORT_UNICODE], [], [
    Define to any value to enable support for Unicode and UTF encoding.
    This will work even in an EBCDIC environment, but it is incompatible
    with the EBCDIC macro. That is, PCRE2 can support *either* EBCDIC
    code *or* ASCII/Unicode, but not both at once.])
fi

if test "$enable_pcre2grep_libz" = "yes"; then
  AC_DEFINE([SUPPORT_LIBZ], [], [
    Define to any value to allow pcre2grep to be linked with libz, so that it is
    able to handle .gz files.])
fi

if test "$enable_pcre2grep_libbz2" = "yes"; then
  AC_DEFINE([SUPPORT_LIBBZ2], [], [
    Define to any value to allow pcre2grep to be linked with libbz2, so that it
    is able to handle .bz2 files.])
fi

if test $with_pcre2grep_bufsize -lt 8192 ; then
  AC_MSG_WARN([$with_pcre2grep_bufsize is too small for --with-pcre2grep-bufsize; using 8192])
  with_pcre2grep_bufsize="8192"
else
  if test $? -gt 1 ; then
  AC_MSG_ERROR([Bad value for --with-pcre2grep-bufsize])
  fi
fi

if test $with_pcre2grep_max_bufsize -lt $with_pcre2grep_bufsize ; then
  with_pcre2grep_max_bufsize="$with_pcre2grep_bufsize"
else
  if test $? -gt 1 ; then
  AC_MSG_ERROR([Bad value for --with-pcre2grep-max-bufsize])
  fi
fi

AC_DEFINE_UNQUOTED([PCRE2GREP_BUFSIZE], [$with_pcre2grep_bufsize], [
  The value of PCRE2GREP_BUFSIZE is the starting size of the buffer used by
  pcre2grep to hold parts of the file it is searching. The buffer will be
  expanded up to PCRE2GREP_MAX_BUFSIZE if necessary, for files containing very
  long lines. The actual amount of memory used by pcre2grep is three times this
  number, because it allows for the buffering of "before" and "after" lines.])

AC_DEFINE_UNQUOTED([PCRE2GREP_MAX_BUFSIZE], [$with_pcre2grep_max_bufsize], [
  The value of PCRE2GREP_MAX_BUFSIZE specifies the maximum size of the buffer
  used by pcre2grep to hold parts of the file it is searching. The actual
  amount of memory used by pcre2grep is three times this number, because it
  allows for the buffering of "before" and "after" lines.])

if test "$enable_pcre2test_libedit" = "yes"; then
  AC_DEFINE([SUPPORT_LIBEDIT], [], [
    Define to any value to allow pcre2test to be linked with libedit.])
  LIBREADLINE="$LIBEDIT"
elif test "$enable_pcre2test_libreadline" = "yes"; then
  AC_DEFINE([SUPPORT_LIBREADLINE], [], [
    Define to any value to allow pcre2test to be linked with libreadline.])
fi

AC_DEFINE_UNQUOTED([NEWLINE_DEFAULT], [$ac_pcre2_newline_value], [
  The value of NEWLINE_DEFAULT determines the default newline character
  sequence. PCRE2 client programs can override this by selecting other values
  at run time. The valid values are 1 (CR), 2 (LF), 3 (CRLF), 4 (ANY),
  5 (ANYCRLF), and 6 (NUL).])

if test "$enable_bsr_anycrlf" = "yes"; then
  AC_DEFINE([BSR_ANYCRLF], [], [
    By default, the \R escape sequence matches any Unicode line ending
    character or sequence of characters. If BSR_ANYCRLF is defined (to any
    value), this is changed so that backslash-R matches only CR, LF, or CRLF.
    The build-time default can be overridden by the user of PCRE2 at runtime.])
fi

if test "$enable_never_backslash_C" = "yes"; then
  AC_DEFINE([NEVER_BACKSLASH_C], [], [
    Defining NEVER_BACKSLASH_C locks out the use of \C in all patterns.])
fi

AC_DEFINE_UNQUOTED([LINK_SIZE], [$with_link_size], [
  The value of LINK_SIZE determines the number of bytes used to store
  links as offsets within the compiled regex. The default is 2, which
  allows for compiled patterns up to 65535 code units long. This covers the
  vast majority of cases. However, PCRE2 can also be compiled to use 3 or 4
  bytes instead. This allows for longer patterns in extreme cases.])

AC_DEFINE_UNQUOTED([PARENS_NEST_LIMIT], [$with_parens_nest_limit], [
  The value of PARENS_NEST_LIMIT specifies the maximum depth of nested
  parentheses (of any kind) in a pattern. This limits the amount of system
  stack that is used while compiling a pattern.])

AC_DEFINE_UNQUOTED([MATCH_LIMIT], [$with_match_limit], [
  The value of MATCH_LIMIT determines the default number of times the
  pcre2_match() function can record a backtrack position during a single
  matching attempt. The value is also used to limit a loop counter in
  pcre2_dfa_match(). There is a runtime interface for setting a different
  limit. The limit exists in order to catch runaway regular expressions that
  take for ever to determine that they do not match. The default is set very
  large so that it does not accidentally catch legitimate cases.])

# --with-match-limit-recursion is an obsolete synonym for --with-match-limit-depth

if test "$with_match_limit_recursion" != "UNSET"; then
cat <<EOF

WARNING: --with-match-limit-recursion is an obsolete option. Please use
  --with-match-limit-depth in future. If both are set, --with-match-limit-depth
  will be used. See also --with-heap-limit.

EOF
if test "$with_match_limit_depth" = "MATCH_LIMIT"; then
  with_match_limit_depth=$with_match_limit_recursion
fi
fi

AC_DEFINE_UNQUOTED([MATCH_LIMIT_DEPTH], [$with_match_limit_depth], [
  The above limit applies to all backtracks, whether or not they are nested. In
  some environments it is desirable to limit the nesting of backtracking (that
  is, the depth of tree that is searched) more strictly, in order to restrict
  the maximum amount of heap memory that is used. The value of
  MATCH_LIMIT_DEPTH provides this facility. To have any useful effect, it must
  be less than the value of MATCH_LIMIT. The default is to use the same value
  as MATCH_LIMIT. There is a runtime method for setting a different limit. In
  the case of pcre2_dfa_match(), this limit controls the depth of the internal
  nested function calls that are used for pattern recursions, lookarounds, and
  atomic groups.])

AC_DEFINE_UNQUOTED([HEAP_LIMIT], [$with_heap_limit], [
  This limits the amount of memory that may be used while matching
  a pattern. It applies to both pcre2_match() and pcre2_dfa_match(). It does
  not apply to JIT matching. The value is in kibibytes (units of 1024 bytes).])

AC_DEFINE([MAX_NAME_SIZE], [32], [
  This limit is parameterized just in case anybody ever wants to
  change it. Care must be taken if it is increased, because it guards
  against integer overflow caused by enormously large patterns.])

AC_DEFINE([MAX_NAME_COUNT], [10000], [
  This limit is parameterized just in case anybody ever wants to
  change it. Care must be taken if it is increased, because it guards
  against integer overflow caused by enormously large patterns.])

AH_VERBATIM([PCRE2_EXP_DEFN], [
/* If you are compiling for a system other than a Unix-like system or
   Win32, and it needs some magic to be inserted before the definition
   of a function that is exported by the library, define this macro to
   contain the relevant magic. If you do not define this macro, a suitable
    __declspec value is used for Windows systems; in other environments
   "extern" is used for a C compiler and "extern C" for a C++ compiler.
   This macro apears at the start of every exported function that is part
   of the external API. It does not appear on functions that are "external"
   in the C sense, but which are internal to the library. */
#undef PCRE2_EXP_DEFN])

if test "$enable_ebcdic" = "yes"; then
  AC_DEFINE_UNQUOTED([EBCDIC], [], [
    If you are compiling for a system that uses EBCDIC instead of ASCII
    character codes, define this macro to any value. When EBCDIC is set, PCRE2
    assumes that all input strings are in EBCDIC. If you do not define this
    macro, PCRE2 will assume input strings are ASCII or UTF-8/16/32 Unicode. It
    is not possible to build a version of PCRE2 that supports both EBCDIC and
    UTF-8/16/32.])
fi

if test "$enable_ebcdic_nl25" = "yes"; then
  AC_DEFINE_UNQUOTED([EBCDIC_NL25], [], [
    In an EBCDIC environment, define this macro to any value to arrange for
    the NL character to be 0x25 instead of the default 0x15. NL plays the role
    that LF does in an ASCII/Unicode environment.])
fi

if test "$enable_valgrind" = "yes"; then
  AC_DEFINE_UNQUOTED([SUPPORT_VALGRIND], [], [
     Define to any value for valgrind support to find invalid memory reads.])
fi

# Platform specific issues
NO_UNDEFINED=
EXPORT_ALL_SYMBOLS=
case $host_os in
  cygwin* | mingw* )
    if test X"$enable_shared" = Xyes; then
      NO_UNDEFINED="-no-undefined"
      EXPORT_ALL_SYMBOLS="-Wl,--export-all-symbols"
    fi
    ;;
esac

# The extra LDFLAGS for each particular library. The libpcre2*_version values
# are m4 variables, assigned above.

EXTRA_LIBPCRE2_8_LDFLAGS="$EXTRA_LIBPCRE2_8_LDFLAGS \
  $NO_UNDEFINED -version-info libpcre2_8_version"

EXTRA_LIBPCRE2_16_LDFLAGS="$EXTRA_LIBPCRE2_16_LDFLAGS \
  $NO_UNDEFINED -version-info libpcre2_16_version"

EXTRA_LIBPCRE2_32_LDFLAGS="$EXTRA_LIBPCRE2_32_LDFLAGS \
  $NO_UNDEFINED -version-info libpcre2_32_version"

EXTRA_LIBPCRE2_POSIX_LDFLAGS="$EXTRA_LIBPCRE2_POSIX_LDFLAGS \
  $NO_UNDEFINED -version-info libpcre2_posix_version"

AC_SUBST(EXTRA_LIBPCRE2_8_LDFLAGS)
AC_SUBST(EXTRA_LIBPCRE2_16_LDFLAGS)
AC_SUBST(EXTRA_LIBPCRE2_32_LDFLAGS)
AC_SUBST(EXTRA_LIBPCRE2_POSIX_LDFLAGS)

# When we run 'make distcheck', use these arguments. Turning off compiler
# optimization makes it run faster.
DISTCHECK_CONFIGURE_FLAGS="CFLAGS='' CXXFLAGS='' --enable-pcre2-16 --enable-pcre2-32 --enable-jit"
AC_SUBST(DISTCHECK_CONFIGURE_FLAGS)

# Check that, if --enable-pcre2grep-libz or --enable-pcre2grep-libbz2 is
# specified, the relevant library is available.

if test "$enable_pcre2grep_libz" = "yes"; then
  if test "$HAVE_ZLIB_H" != "1"; then
    echo "** Cannot --enable-pcre2grep-libz because zlib.h was not found"
    exit 1
  fi
  if test "$HAVE_LIBZ" != "1"; then
    echo "** Cannot --enable-pcre2grep-libz because libz was not found"
    exit 1
  fi
  LIBZ="-lz"
fi
AC_SUBST(LIBZ)

if test "$enable_pcre2grep_libbz2" = "yes"; then
  if test "$HAVE_BZLIB_H" != "1"; then
    echo "** Cannot --enable-pcre2grep-libbz2 because bzlib.h was not found"
    exit 1
  fi
  if test "$HAVE_LIBBZ2" != "1"; then
    echo "** Cannot --enable-pcre2grep-libbz2 because libbz2 was not found"
    exit 1
  fi
  LIBBZ2="-lbz2"
fi
AC_SUBST(LIBBZ2)

# Similarly for --enable-pcre2test-readline

if test "$enable_pcre2test_libedit" = "yes"; then
  if test "$enable_pcre2test_libreadline" = "yes"; then
    echo "** Cannot use both --enable-pcre2test-libedit and --enable-pcre2test-readline"
    exit 1
  fi
  if test "$HAVE_EDITLINE_READLINE_H" != "1" -a \
          "$HAVE_READLINE_READLINE_H" != "1"; then
    echo "** Cannot --enable-pcre2test-libedit because neither editline/readline.h"
    echo "** nor readline/readline.h was found."
    exit 1
  fi
  if test -z "$LIBEDIT"; then
    echo "** Cannot --enable-pcre2test-libedit because libedit library was not found."
    exit 1
  fi
fi

if test "$enable_pcre2test_libreadline" = "yes"; then
  if test "$HAVE_READLINE_H" != "1"; then
    echo "** Cannot --enable-pcre2test-readline because readline/readline.h was not found."
    exit 1
  fi
  if test "$HAVE_HISTORY_H" != "1"; then
    echo "** Cannot --enable-pcre2test-readline because readline/history.h was not found."
    exit 1
  fi
  if test -z "$LIBREADLINE"; then
    echo "** Cannot --enable-pcre2test-readline because readline library was not found."
    exit 1
  fi
fi

# Handle valgrind support

if test "$enable_valgrind" = "yes"; then
  m4_ifdef([PKG_CHECK_MODULES],
           [PKG_CHECK_MODULES([VALGRIND],[valgrind])],
           [AC_MSG_ERROR([pkg-config not supported])])
fi

# Handle code coverage reporting support
if test "$enable_coverage" = "yes"; then
  if test "x$GCC" != "xyes"; then
    AC_MSG_ERROR([Code coverage reports can only be generated when using GCC])
  fi

  # ccache is incompatible with gcov
  AC_PATH_PROG([SHTOOL],[shtool],[false])
  case `$SHTOOL path $CC` in
    *ccache*) cc_ccache=yes;;
    *) cc_ccache=no;;
  esac

  if test "$cc_ccache" = "yes"; then
    if test -z "$CCACHE_DISABLE" -o "$CCACHE_DISABLE" != "1"; then
      AC_MSG_ERROR([must export CCACHE_DISABLE=1 to disable ccache for code coverage])
    fi
  fi

  AC_ARG_VAR([LCOV],[the ltp lcov program])
  AC_PATH_PROG([LCOV],[lcov],[false])
  if test "x$LCOV" = "xfalse"; then
    AC_MSG_ERROR([lcov not found])
  fi

  AC_ARG_VAR([GENHTML],[the ltp genhtml program])
  AC_PATH_PROG([GENHTML],[genhtml],[false])
  if test "x$GENHTML" = "xfalse"; then
    AC_MSG_ERROR([genhtml not found])
  fi

  # Set flags needed for gcov
  GCOV_CFLAGS="-O0 -ggdb3 -fprofile-arcs -ftest-coverage"
  GCOV_CXXFLAGS="-O0 -ggdb3 -fprofile-arcs -ftest-coverage"
  GCOV_LIBS="-lgcov"
  AC_SUBST([GCOV_CFLAGS])
  AC_SUBST([GCOV_CXXFLAGS])
  AC_SUBST([GCOV_LIBS])
fi # enable_coverage

AM_CONDITIONAL([WITH_GCOV],[test "x$enable_coverage" = "xyes"])

# Produce these files, in addition to config.h.
AC_CONFIG_FILES(
	Makefile
	libpcre2-8.pc
	libpcre2-16.pc
	libpcre2-32.pc
	libpcre2-posix.pc
	pcre2-config
	src/pcre2.h
)

# Make the generated script files executable.
AC_CONFIG_COMMANDS([script-chmod], [chmod a+x pcre2-config])

# Make sure that pcre2_chartables.c is removed in case the method for
# creating it was changed by reconfiguration.
AC_CONFIG_COMMANDS([delete-old-chartables], [rm -f pcre2_chartables.c])

AC_OUTPUT

# --disable-stack-for-recursion is obsolete and has no effect.

if test "$enable_stack_for_recursion" = "no"; then
cat <<EOF

WARNING: --disable-stack-for-recursion is obsolete and has no effect.
EOF
fi

# Print out a nice little message after configure is run displaying the
# chosen options.

ebcdic_nl_code=n/a
if test "$enable_ebcdic_nl25" = "yes"; then
  ebcdic_nl_code=0x25
elif test "$enable_ebcdic" = "yes"; then
  ebcdic_nl_code=0x15
fi

cat <<EOF

$PACKAGE-$VERSION configuration summary:

    Install prefix ..................... : ${prefix}
    C preprocessor ..................... : ${CPP}
    C compiler ......................... : ${CC}
    Linker ............................. : ${LD}
    C preprocessor flags ............... : ${CPPFLAGS}
    C compiler flags ................... : ${CFLAGS} ${VISIBILITY_CFLAGS}
    Linker flags ....................... : ${LDFLAGS}
    Extra libraries .................... : ${LIBS}

    Build 8-bit pcre2 library .......... : ${enable_pcre2_8}
    Build 16-bit pcre2 library ......... : ${enable_pcre2_16}
    Build 32-bit pcre2 library ......... : ${enable_pcre2_32}
    Include debugging code ............. : ${enable_debug}
    Enable JIT compiling support ....... : ${enable_jit}
    Use SELinux allocator in JIT ....... : ${enable_jit_sealloc}
    Enable Unicode support ............. : ${enable_unicode}
    Newline char/sequence .............. : ${enable_newline}
    \R matches only ANYCRLF ............ : ${enable_bsr_anycrlf}
    \C is disabled ..................... : ${enable_never_backslash_C}
    EBCDIC coding ...................... : ${enable_ebcdic}
    EBCDIC code for NL ................. : ${ebcdic_nl_code}
    Rebuild char tables ................ : ${enable_rebuild_chartables}
    Internal link size ................. : ${with_link_size}
    Nested parentheses limit ........... : ${with_parens_nest_limit}
    Heap limit ......................... : ${with_heap_limit} kibibytes
    Match limit ........................ : ${with_match_limit}
    Match depth limit .................. : ${with_match_limit_depth}
    Build shared libs .................. : ${enable_shared}
    Build static libs .................. : ${enable_static}
    Use JIT in pcre2grep ............... : ${enable_pcre2grep_jit}
    Enable callouts in pcre2grep ....... : ${enable_pcre2grep_callout}
    Initial buffer size for pcre2grep .. : ${with_pcre2grep_bufsize}
    Maximum buffer size for pcre2grep .. : ${with_pcre2grep_max_bufsize}
    Link pcre2grep with libz ........... : ${enable_pcre2grep_libz}
    Link pcre2grep with libbz2 ......... : ${enable_pcre2grep_libbz2}
    Link pcre2test with libedit ........ : ${enable_pcre2test_libedit}
    Link pcre2test with libreadline .... : ${enable_pcre2test_libreadline}
    Valgrind support ................... : ${enable_valgrind}
    Code coverage ...................... : ${enable_coverage}
    Fuzzer support ..................... : ${enable_fuzz_support}

EOF

dnl end configure.ac
