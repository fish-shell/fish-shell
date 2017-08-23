# ======================================================================================
# Author: Francesco Montorsi
# RCS-ID: $Id: configure.ac 634 2012-12-22 10:34:32Z ibg $
# ======================================================================================


# NOTE:
#  the version of the project must be updated also in docs/Doxyfile !
AC_INIT([muparser], [2.2.4], [muparser@beltoforion.de])

AC_PREREQ([2.57])
AC_CONFIG_AUX_DIR([build/autoconf])
AC_LANG(C++)

# some helpers
m4_include(bakefile-presets.m4)



## CONFIGURE CHECKS
#####################################################################

dnl This allows us to use Bakefile, recognizing the system type
dnl (and sets the AC_CANONICAL_BUILD, AC_CANONICAL_HOST and
dnl AC_CANONICAL_TARGET variables)
AC_CANONICAL_SYSTEM

# We want to inhibit AC_PROG_C* macros adding the default "-g -O2" flags.
# To do so, make sure that the variables are not unset - it is ok if they are
# empty. However, their value MUST be retained, since one may have
# called ./configure CFLAGS=-foobar.
CFLAGS="$CFLAGS"
CPPFLAGS="$CPPFLAGS"
CXXFLAGS="$CXXFLAGS"

dnl Checks for basic programs used to compile/install.
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CXXCPP


AC_BAKEFILE_DEBUGOPT([no])
AC_BAKEFILE_SHAREDOPT([yes])

AC_ARG_ENABLE([samples],
            AC_HELP_STRING([--enable-samples], [Builds the library samples, too]),
            [], [enableval="yes"])
AC_BAKEFILE_YESNO_OPTCHECK([SAMPLES], [enableval], [--enable-samples])

dnl Create the output folders in the current build directory
dnl (this handles cases like:
dnl      mkdir mybuild && cd mybuild && ../configure && make  )
AS_MKDIR_P(lib)
AS_MKDIR_P(samples/example1)

AC_BAKEFILE([m4_include(autoconf_inc.m4)])
AC_CONFIG_FILES([Makefile build/autoconf/muparser.pc])
AC_OUTPUT



## CONFIGURE END MESSAGE
#####################################################################

echo
echo " ----------------------------------------------------------------"
echo "  Configuration for $PACKAGE_NAME $PACKAGE_VERSION successfully completed."
echo "  Summary of main configuration settings for $PACKAGE_NAME:"
AC_BAKEFILE_SHOW_SHAREDOPT
AC_BAKEFILE_SHOW_DEBUGOPT
if [[ "$SAMPLES" = "1" ]]; then
    echo "  - SAMPLES enabled"
else
    echo "  - SAMPLES disabled"
fi
echo "  Now, just run make."
echo " ----------------------------------------------------------------"
echo
