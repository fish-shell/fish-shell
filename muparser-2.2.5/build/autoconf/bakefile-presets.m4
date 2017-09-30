dnl ---------------------------------------------------------------------------
dnl Support macros for makefiles generated with Bakefile presets
dnl ---------------------------------------------------------------------------


dnl ---------------------------------------------------------------------------
dnl AM_YESNO_OPTCHECK([name of the boolean variable to set],
dnl                   [name of the variable with yes/no values],
dnl                   [name of the --enable/--with option])
dnl
dnl Converts the $3 variable, supposed to contain a yes/no value to a 1/0
dnl boolean variable and saves the result into $1.
dnl Outputs also the standard checking-option message.
dnl Used by the m4 macros of the presets.
dnl ---------------------------------------------------------------------------
AC_DEFUN([AC_BAKEFILE_YESNO_OPTCHECK],
[
    AC_MSG_CHECKING([for the $3 option])
    if [[ "x$$2" = "xyes" ]]; then
        AC_MSG_RESULT([yes])
        $1=1
    else
        AC_MSG_RESULT([no])
        $1=0
    fi
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_UNICODEOPT([default value for the --enable-unicode option])
dnl
dnl Adds the --enable-unicode option to the configure script and sets the
dnl UNICODE=0/1 variable accordingly to the value of the option.
dnl To be used with unicodeopt.bkl preset.
dnl ---------------------------------------------------------------------------
AC_DEFUN([AC_BAKEFILE_UNICODEOPT],
[
    default="$1"
    if [[ -z "$default" ]]; then
        default="no"
    fi

    AC_ARG_ENABLE([unicode],
            AC_HELP_STRING([--enable-unicode], [Builds in Unicode mode]),
            [], [enableval="$default"])

    AC_BAKEFILE_YESNO_OPTCHECK([UNICODE], [enableval], [--enable-unicode])
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_DEBUGOPT([default value for the --enable-debug option])
dnl
dnl Adds the --enable-debug option to the configure script and sets the
dnl DEBUG=0/1 variable accordingly to the value of the option.
dnl To be used with debugopt.bkl preset.
dnl ---------------------------------------------------------------------------
AC_DEFUN([AC_BAKEFILE_DEBUGOPT],
[
    default="$1"
    if [[ -z "$default" ]]; then
        default="no"
    fi

    AC_ARG_ENABLE([debug],
            AC_HELP_STRING([--enable-debug], [Builds in debug mode]),
            [], [enableval="$default"])

    AC_BAKEFILE_YESNO_OPTCHECK([DEBUG], [enableval], [--enable-debug])

    dnl add the optimize/debug flags
    if [[ "x$DEBUG" = "x1" ]]; then

        dnl NOTE: the -Wundef and -Wno-ctor-dtor-privacy are not enabled automatically by -Wall
        dnl NOTE2: the '-Wno-ctor-dtor-privacy' has sense only when compiling C++ source files
        dnl        and thus we must be careful to add it only to CXXFLAGS and not to CFLAGS
        dnl        (remember that CPPFLAGS is reserved for both C and C++ compilers while
        dnl         CFLAGS is intended as flags for C compiler only and CXXFLAGS for C++ only)
        my_CXXFLAGS="$my_CXXFLAGS -g -O0 -Wall -Wundef -Wno-ctor-dtor-privacy"
        my_CFLAGS="$my_CFLAGS -g -O0 -Wall -Wundef"
    else
        my_CXXFLAGS="$my_CXXFLAGS -O2"
        my_CFLAGS="$my_CFLAGS -O2"
    fi
    # User-supplied CXXFLAGS must always take precedence.
    # This still sucks because using `make CFLAGS=-foobar` kills
    # the project-supplied flags again.
    CXXFLAGS="$my_CXXFLAGS $CXXFLAGS"
    CFLAGS="$my_CFLAGS $CFLAGS"
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_SHAREDOPT([default value for the --enable-shared option])
dnl
dnl Adds the --enable-shared option to the configure script and sets the
dnl SHARED=0/1 variable accordingly to the value of the option.
dnl To be used with sharedopt.bkl preset.
dnl ---------------------------------------------------------------------------
AC_DEFUN([AC_BAKEFILE_SHAREDOPT],
[
    default="$1"
    if [[ -z "$default" ]]; then
        default="no"
    fi

    AC_ARG_ENABLE([shared],
            AC_HELP_STRING([--enable-shared], [Builds in shared mode]),
            [], [enableval="$default"])

    AC_BAKEFILE_YESNO_OPTCHECK([SHARED], [enableval], [--enable-shared])
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_SHOW_DEBUGOPT
dnl
dnl Prints a message on stdout about the value of the DEBUG variable.
dnl This macro is useful to show summary messages at the end of the configure scripts.
dnl ---------------------------------------------------------------------------
AC_DEFUN([AC_BAKEFILE_SHOW_DEBUGOPT],
[
    if [[ "$DEBUG" = "1" ]]; then
        echo "  - DEBUG build"
    else
        echo "  - RELEASE build"
    fi
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_SHOW_SHAREDOPT
dnl
dnl Prints a message on stdout about the value of the SHARED variable.
dnl This macro is useful to show summary messages at the end of the configure scripts.
dnl ---------------------------------------------------------------------------
AC_DEFUN([AC_BAKEFILE_SHOW_SHAREDOPT],
[
    if [[ "$SHARED" = "1" ]]; then
        echo "  - SHARED mode"
    else
        echo "  - STATIC mode"
    fi
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_SHOW_UNICODEOPT
dnl
dnl Prints a message on stdout about the value of the UNICODE variable.
dnl This macro is useful to show summary messages at the end of the configure scripts.
dnl ---------------------------------------------------------------------------
AC_DEFUN([AC_BAKEFILE_SHOW_UNICODEOPT],
[
    if [[ "$UNICODE" = "1" ]]; then
        echo "  - UNICODE mode"
    else
        echo "  - ANSI mode"
    fi
])

 	  	 
