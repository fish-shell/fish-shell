# generated automatically by aclocal 1.13.3 -*- Autoconf -*-

# Copyright (C) 1996-2013 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

m4_ifndef([AC_CONFIG_MACRO_DIRS], [m4_defun([_AM_CONFIG_MACRO_DIRS], [])m4_defun([AC_CONFIG_MACRO_DIRS], [_AM_CONFIG_MACRO_DIRS($@)])])
AC_DEFUN([AC_BAKEFILE_CREATE_FILE_DLLAR_SH],
[
dnl ===================== dllar.sh begins here =====================
dnl    (Created by merge-scripts.py from dllar.sh
dnl     file do not edit here!)
D='$'
cat <<EOF >dllar.sh
#!/bin/sh
#
# dllar - a tool to build both a .dll and an .a file
# from a set of object (.o) files for EMX/OS2.
#
#  Written by Andrew Zabolotny, bit@freya.etu.ru
#  Ported to Unix like shell by Stefan Neis, Stefan.Neis@t-online.de
#
#  This script will accept a set of files on the command line.
#  All the public symbols from the .o files will be exported into
#  a .DEF file, then linker will be run (through gcc) against them to
#  build a shared library consisting of all given .o files. All libraries
#  (.a) will be first decompressed into component .o files then act as
#  described above. You can optionally give a description (-d "description")
#  which will be put into .DLL. To see the list of accepted options (as well
#  as command-line format) simply run this program without options. The .DLL
#  is built to be imported by name (there is no guarantee that new versions
#  of the library you build will have same ordinals for same symbols).
#
#  dllar is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  dllar is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with dllar; see the file COPYING.  If not, write to the Free
#  Software Foundation, 59 Temple Place - Suite 330, Boston, MA
#  02111-1307, USA.

# To successfuly run this program you will need:
#  - Current drive should have LFN support (HPFS, ext2, network, etc)
#    (Sometimes dllar generates filenames which won't fit 8.3 scheme)
#  - gcc
#    (used to build the .dll)
#  - emxexp
#    (used to create .def file from .o files)
#  - emximp
#    (used to create .a file from .def file)
#  - GNU text utilites (cat, sort, uniq)
#    used to process emxexp output
#  - GNU file utilities (mv, rm)
#  - GNU sed
#  - lxlite (optional, see flag below)
#    (used for general .dll cleanup)
#

flag_USE_LXLITE=1;

#
# helper functions
# basnam, variant of basename, which does _not_ remove the path, _iff_
#                              second argument (suffix to remove) is given
basnam(){
    case ${D}# in
    1)
        echo ${D}1 | sed 's/.*\\///' | sed 's/.*\\\\//'
        ;;
    2)
        echo ${D}1 | sed 's/'${D}2'${D}//'
        ;;
    *)
        echo "error in basnam ${D}*"
        exit 8
        ;;
    esac
}

# Cleanup temporary files and output
CleanUp() {
    cd ${D}curDir
    for i in ${D}inputFiles ; do
        case ${D}i in
        *!)
            rm -rf \`basnam ${D}i !\`
            ;;
        *)
            ;;
        esac
    done

    # Kill result in case of failure as there is just to many stupid make/nmake
    # things out there which doesn't do this.
    if @<:@ ${D}# -eq 0 @:>@; then
        rm -f ${D}arcFile ${D}arcFile2 ${D}defFile ${D}dllFile
    fi
}

# Print usage and exit script with rc=1.
PrintHelp() {
 echo 'Usage: dllar.sh @<:@-o@<:@utput@:>@ output_file@:>@ @<:@-i@<:@mport@:>@ importlib_name@:>@'
 echo '       @<:@-name-mangler-script script.sh@:>@'
 echo '       @<:@-d@<:@escription@:>@ "dll descrption"@:>@ @<:@-cc "CC"@:>@ @<:@-f@<:@lags@:>@ "CFLAGS"@:>@'
 echo '       @<:@-ord@<:@inals@:>@@:>@ -ex@<:@clude@:>@ "symbol(s)"'
 echo '       @<:@-libf@<:@lags@:>@ "{INIT|TERM}{GLOBAL|INSTANCE}"@:>@ @<:@-nocrt@<:@dll@:>@@:>@ @<:@-nolxl@<:@ite@:>@@:>@'
 echo '       @<:@*.o@:>@ @<:@*.a@:>@'
 echo '*> "output_file" should have no extension.'
 echo '   If it has the .o, .a or .dll extension, it is automatically removed.'
 echo '   The import library name is derived from this and is set to "name".a,'
 echo '   unless overridden by -import'
 echo '*> "importlib_name" should have no extension.'
 echo '   If it has the .o, or .a extension, it is automatically removed.'
 echo '   This name is used as the import library name and may be longer and'
 echo '   more descriptive than the DLL name which has to follow the old '
 echo '   8.3 convention of FAT.'
 echo '*> "script.sh may be given to override the output_file name by a'
 echo '   different name. It is mainly useful if the regular make process'
 echo '   of some package does not take into account OS/2 restriction of'
 echo '   DLL name lengths. It takes the importlib name as input and is'
 echo '   supposed to procude a shorter name as output. The script should'
 echo '   expect to get importlib_name without extension and should produce'
 echo '   a (max.) 8 letter name without extension.'
 echo '*> "cc" is used to use another GCC executable.   (default: gcc.exe)'
 echo '*> "flags" should be any set of valid GCC flags. (default: -s -Zcrtdll)'
 echo '   These flags will be put at the start of GCC command line.'
 echo '*> -ord@<:@inals@:>@ tells dllar to export entries by ordinals. Be careful.'
 echo '*> -ex@<:@clude@:>@ defines symbols which will not be exported. You can define'
 echo '   multiple symbols, for example -ex "myfunc yourfunc _GLOBAL*".'
 echo '   If the last character of a symbol is "*", all symbols beginning'
 echo '   with the prefix before "*" will be exclude, (see _GLOBAL* above).'
 echo '*> -libf@<:@lags@:>@ can be used to add INITGLOBAL/INITINSTANCE and/or'
 echo '   TERMGLOBAL/TERMINSTANCE flags to the dynamically-linked library.'
 echo '*> -nocrt@<:@dll@:>@ switch will disable linking the library against emx''s'
 echo '   C runtime DLLs.'
 echo '*> -nolxl@<:@ite@:>@ switch will disable running lxlite on the resulting DLL.'
 echo '*> All other switches (for example -L./ or -lmylib) will be passed'
 echo '   unchanged to GCC at the end of command line.'
 echo '*> If you create a DLL from a library and you do not specify -o,'
 echo '   the basename for DLL and import library will be set to library name,'
 echo '   the initial library will be renamed to 'name'_s.a (_s for static)'
 echo '   i.e. "dllar gcc.a" will create gcc.dll and gcc.a, and the initial'
 echo '   library will be renamed into gcc_s.a.'
 echo '--------'
 echo 'Example:'
 echo '   dllar -o gcc290.dll libgcc.a -d "GNU C runtime library" -ord'
 echo '    -ex "__main __ctordtor*" -libf "INITINSTANCE TERMINSTANCE"'
 CleanUp
 exit 1
}

# Execute a command.
# If exit code of the commnad <> 0 CleanUp() is called and we'll exit the script.
# @Uses    Whatever CleanUp() uses.
doCommand() {
    echo "${D}*"
    eval ${D}*
    rcCmd=${D}?

    if @<:@ ${D}rcCmd -ne 0 @:>@; then
        echo "command failed, exit code="${D}rcCmd
        CleanUp
        exit ${D}rcCmd
    fi
}

# main routine
# setup globals
cmdLine=${D}*
outFile=""
outimpFile=""
inputFiles=""
renameScript=""
description=""
CC=gcc.exe
CFLAGS="-s -Zcrtdll"
EXTRA_CFLAGS=""
EXPORT_BY_ORDINALS=0
exclude_symbols=""
library_flags=""
curDir=\`pwd\`
curDirS=curDir
case ${D}curDirS in
*/)
  ;;
*)
  curDirS=${D}{curDirS}"/"
  ;;
esac
# Parse commandline
libsToLink=0
omfLinking=0
while @<:@ ${D}1 @:>@; do
    case ${D}1 in
    -ord*)
        EXPORT_BY_ORDINALS=1;
        ;;
    -o*)
	shift
        outFile=${D}1
	;;
    -i*)
        shift
        outimpFile=${D}1
        ;;
    -name-mangler-script)
        shift
        renameScript=${D}1
        ;;
    -d*)
        shift
        description=${D}1
        ;;
    -f*)
        shift
        CFLAGS=${D}1
        ;;
    -c*)
        shift
        CC=${D}1
        ;;
    -h*)
        PrintHelp
        ;;
    -ex*)
        shift
        exclude_symbols=${D}{exclude_symbols}${D}1" "
        ;;
    -libf*)
        shift
        library_flags=${D}{library_flags}${D}1" "
        ;;
    -nocrt*)
        CFLAGS="-s"
        ;;
    -nolxl*)
        flag_USE_LXLITE=0
        ;;
    -* | /*)
        case ${D}1 in
        -L* | -l*)
            libsToLink=1
            ;;
        -Zomf)
            omfLinking=1
            ;;
        *)
            ;;
        esac
        EXTRA_CFLAGS=${D}{EXTRA_CFLAGS}" "${D}1
        ;;
    *.dll)
        EXTRA_CFLAGS="${D}{EXTRA_CFLAGS} \`basnam ${D}1 .dll\`"
        if @<:@ ${D}omfLinking -eq 1 @:>@; then
            EXTRA_CFLAGS="${D}{EXTRA_CFLAGS}.lib"
	else
            EXTRA_CFLAGS="${D}{EXTRA_CFLAGS}.a"
        fi
        ;;
    *)
        found=0;
        if @<:@ ${D}libsToLink -ne 0 @:>@; then
            EXTRA_CFLAGS=${D}{EXTRA_CFLAGS}" "${D}1
        else
            for file in ${D}1 ; do
                if @<:@ -f ${D}file @:>@; then
                    inputFiles="${D}{inputFiles} ${D}file"
                    found=1
                fi
            done
            if @<:@ ${D}found -eq 0 @:>@; then
                echo "ERROR: No file(s) found: "${D}1
                exit 8
            fi
        fi
      ;;
    esac
    shift
done # iterate cmdline words

#
if @<:@ -z "${D}inputFiles" @:>@; then
    echo "dllar: no input files"
    PrintHelp
fi

# Now extract all .o files from .a files
newInputFiles=""
for file in ${D}inputFiles ; do
    case ${D}file in
    *.a | *.lib)
        case ${D}file in
        *.a)
            suffix=".a"
            AR="ar"
            ;;
        *.lib)
            suffix=".lib"
            AR="emxomfar"
            EXTRA_CFLAGS="${D}EXTRA_CFLAGS -Zomf"
            ;;
        *)
            ;;
        esac
        dirname=\`basnam ${D}file ${D}suffix\`"_%"
        mkdir ${D}dirname
        if @<:@ ${D}? -ne 0 @:>@; then
            echo "Failed to create subdirectory ./${D}dirname"
            CleanUp
            exit 8;
        fi
        # Append '!' to indicate archive
        newInputFiles="${D}newInputFiles ${D}{dirname}!"
        doCommand "cd ${D}dirname; ${D}AR x ../${D}file"
        cd ${D}curDir
        found=0;
        for subfile in ${D}dirname/*.o* ; do
            if @<:@ -f ${D}subfile @:>@; then
                found=1
                if @<:@ -s ${D}subfile @:>@; then
	            # FIXME: This should be: is file size > 32 byte, _not_ > 0!
                    newInputFiles="${D}newInputFiles ${D}subfile"
                fi
            fi
        done
        if @<:@ ${D}found -eq 0 @:>@; then
            echo "WARNING: there are no files in archive \\'${D}file\\'"
        fi
        ;;
    *)
        newInputFiles="${D}{newInputFiles} ${D}file"
        ;;
    esac
done
inputFiles="${D}newInputFiles"

# Output filename(s).
do_backup=0;
if @<:@ -z ${D}outFile @:>@; then
    do_backup=1;
    set outFile ${D}inputFiles; outFile=${D}2
fi

# If it is an archive, remove the '!' and the '_%' suffixes
case ${D}outFile in
*_%!)
    outFile=\`basnam ${D}outFile _%!\`
    ;;
*)
    ;;
esac
case ${D}outFile in
*.dll)
    outFile=\`basnam ${D}outFile .dll\`
    ;;
*.DLL)
    outFile=\`basnam ${D}outFile .DLL\`
    ;;
*.o)
    outFile=\`basnam ${D}outFile .o\`
    ;;
*.obj)
    outFile=\`basnam ${D}outFile .obj\`
    ;;
*.a)
    outFile=\`basnam ${D}outFile .a\`
    ;;
*.lib)
    outFile=\`basnam ${D}outFile .lib\`
    ;;
*)
    ;;
esac
case ${D}outimpFile in
*.a)
    outimpFile=\`basnam ${D}outimpFile .a\`
    ;;
*.lib)
    outimpFile=\`basnam ${D}outimpFile .lib\`
    ;;
*)
    ;;
esac
if @<:@ -z ${D}outimpFile @:>@; then
    outimpFile=${D}outFile
fi
defFile="${D}{outFile}.def"
arcFile="${D}{outimpFile}.a"
arcFile2="${D}{outimpFile}.lib"

#create ${D}dllFile as something matching 8.3 restrictions,
if @<:@ -z ${D}renameScript @:>@ ; then
    dllFile="${D}outFile"
else
    dllFile=\`${D}renameScript ${D}outimpFile\`
fi

if @<:@ ${D}do_backup -ne 0 @:>@ ; then
    if @<:@ -f ${D}arcFile @:>@ ; then
        doCommand "mv ${D}arcFile ${D}{outFile}_s.a"
    fi
    if @<:@ -f ${D}arcFile2 @:>@ ; then
        doCommand "mv ${D}arcFile2 ${D}{outFile}_s.lib"
    fi
fi

# Extract public symbols from all the object files.
tmpdefFile=${D}{defFile}_%
rm -f ${D}tmpdefFile
for file in ${D}inputFiles ; do
    case ${D}file in
    *!)
        ;;
    *)
        doCommand "emxexp -u ${D}file >> ${D}tmpdefFile"
        ;;
    esac
done

# Create the def file.
rm -f ${D}defFile
echo "LIBRARY \`basnam ${D}dllFile\` ${D}library_flags" >> ${D}defFile
dllFile="${D}{dllFile}.dll"
if @<:@ ! -z ${D}description @:>@; then
    echo "DESCRIPTION  \\"${D}{description}\\"" >> ${D}defFile
fi
echo "EXPORTS" >> ${D}defFile

doCommand "cat ${D}tmpdefFile | sort.exe | uniq.exe > ${D}{tmpdefFile}%"
grep -v "^ *;" < ${D}{tmpdefFile}% | grep -v "^ *${D}" >${D}tmpdefFile

# Checks if the export is ok or not.
for word in ${D}exclude_symbols; do
    grep -v ${D}word < ${D}tmpdefFile >${D}{tmpdefFile}%
    mv ${D}{tmpdefFile}% ${D}tmpdefFile
done


if @<:@ ${D}EXPORT_BY_ORDINALS -ne 0 @:>@; then
    sed "=" < ${D}tmpdefFile | \\
    sed '
      N
      : loop
      s/^\\(@<:@0-9@:>@\\+\\)\\(@<:@^;@:>@*\\)\\(;.*\\)\\?/\\2 @\\1 NONAME/
      t loop
    ' > ${D}{tmpdefFile}%
    grep -v "^ *${D}" < ${D}{tmpdefFile}% > ${D}tmpdefFile
else
    rm -f ${D}{tmpdefFile}%
fi
cat ${D}tmpdefFile >> ${D}defFile
rm -f ${D}tmpdefFile

# Do linking, create implib, and apply lxlite.
gccCmdl="";
for file in ${D}inputFiles ; do
    case ${D}file in
    *!)
        ;;
    *)
        gccCmdl="${D}gccCmdl ${D}file"
        ;;
    esac
done
doCommand "${D}CC ${D}CFLAGS -Zdll -o ${D}dllFile ${D}defFile ${D}gccCmdl ${D}EXTRA_CFLAGS"
touch "${D}{outFile}.dll"

doCommand "emximp -o ${D}arcFile ${D}defFile"
if @<:@ ${D}flag_USE_LXLITE -ne 0 @:>@; then
    add_flags="";
    if @<:@ ${D}EXPORT_BY_ORDINALS -ne 0 @:>@; then
        add_flags="-ynd"
    fi
    doCommand "lxlite -cs -t: -mrn -mln ${D}add_flags ${D}dllFile"
fi
doCommand "emxomf -s -l ${D}arcFile"

# Successful exit.
CleanUp 1
exit 0
EOF
dnl ===================== dllar.sh ends here =====================
])

dnl
dnl  This file is part of Bakefile (http://www.bakefile.org)
dnl
dnl  Copyright (C) 2003-2007 Vaclav Slavik, David Elliott and others
dnl
dnl  Permission is hereby granted, free of charge, to any person obtaining a
dnl  copy of this software and associated documentation files (the "Software"),
dnl  to deal in the Software without restriction, including without limitation
dnl  the rights to use, copy, modify, merge, publish, distribute, sublicense,
dnl  and/or sell copies of the Software, and to permit persons to whom the
dnl  Software is furnished to do so, subject to the following conditions:
dnl
dnl  The above copyright notice and this permission notice shall be included in
dnl  all copies or substantial portions of the Software.
dnl
dnl  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
dnl  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
dnl  DEALINGS IN THE SOFTWARE.
dnl
dnl  $Id: bakefile-lang.m4 1337 2010-02-09 20:22:43Z vaclavslavik $
dnl
dnl  Compiler detection macros by David Elliott and Vadim Zeitlin
dnl


dnl ===========================================================================
dnl Macros to detect different C/C++ compilers
dnl ===========================================================================

dnl Based on autoconf _AC_LANG_COMPILER_GNU
dnl _AC_BAKEFILE_LANG_COMPILER(NAME, LANG, SYMBOL, IF-YES, IF-NO)
AC_DEFUN([_AC_BAKEFILE_LANG_COMPILER],
[
    AC_LANG_PUSH($2)
    AC_CACHE_CHECK(
        [whether we are using the $1 $2 compiler],
        [bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3],
        [AC_TRY_COMPILE(
            [],
            [
             #ifndef $3
                choke me
             #endif
            ],
            [bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3=yes],
            [bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3=no]
         )
        ]
    )
    if test "x$bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3" = "xyes"; then
        :; $4
    else
        :; $5
    fi
    AC_LANG_POP($2)
])

dnl More specific version of the above macro checking whether the compiler
dnl version is at least the given one (assumes that we do use this compiler)
dnl
dnl _AC_BAKEFILE_LANG_COMPILER_LATER_THAN(NAME, LANG, SYMBOL, VER, VERMSG, IF-YES, IF-NO)
AC_DEFUN([_AC_BAKEFILE_LANG_COMPILER_LATER_THAN],
[
    AC_LANG_PUSH($2)
    AC_CACHE_CHECK(
        [whether we are using $1 $2 compiler v$5 or later],
        [bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3[]_lt_[]$4],
        [AC_TRY_COMPILE(
            [],
            [
             #ifndef $3 || $3 < $4
                choke me
             #endif
            ],
            [bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3[]_lt_[]$4=yes],
            [bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3[]_lt_[]$4=no]
         )
        ]
    )
    if test "x$bakefile_cv_[]_AC_LANG_ABBREV[]_compiler_[]$3[]_lt_[]$4" = "xyes"; then
        :; $6
    else
        :; $7
    fi
    AC_LANG_POP($2)
])

dnl CodeWarrior Metrowerks compiler defines __MWERKS__ for both C and C++
AC_DEFUN([AC_BAKEFILE_PROG_MWCC],
[
    _AC_BAKEFILE_LANG_COMPILER(Metrowerks, C, __MWERKS__, MWCC=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_MWCXX],
[
    _AC_BAKEFILE_LANG_COMPILER(Metrowerks, C++, __MWERKS__, MWCXX=yes)
])

dnl IBM xlC compiler defines __xlC__ for both C and C++
AC_DEFUN([AC_BAKEFILE_PROG_XLCC],
[
    _AC_BAKEFILE_LANG_COMPILER([IBM xlC], C, __xlC__, XLCC=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_XLCXX],
[
    _AC_BAKEFILE_LANG_COMPILER([IBM xlC], C++, __xlC__, XLCXX=yes)
])

dnl recent versions of SGI mipsPro compiler define _SGI_COMPILER_VERSION
dnl
dnl NB: old versions define _COMPILER_VERSION but this could probably be
dnl     defined by other compilers too so don't test for it to be safe
AC_DEFUN([AC_BAKEFILE_PROG_SGICC],
[
    _AC_BAKEFILE_LANG_COMPILER(SGI, C, _SGI_COMPILER_VERSION, SGICC=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_SGICXX],
[
    _AC_BAKEFILE_LANG_COMPILER(SGI, C++, _SGI_COMPILER_VERSION, SGICXX=yes)
])

dnl Sun compiler defines __SUNPRO_C/__SUNPRO_CC
AC_DEFUN([AC_BAKEFILE_PROG_SUNCC],
[
    _AC_BAKEFILE_LANG_COMPILER(Sun, C, __SUNPRO_C, SUNCC=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_SUNCXX],
[
    _AC_BAKEFILE_LANG_COMPILER(Sun, C++, __SUNPRO_CC, SUNCXX=yes)
])

dnl Intel icc compiler defines __INTEL_COMPILER for both C and C++
AC_DEFUN([AC_BAKEFILE_PROG_INTELCC],
[
    _AC_BAKEFILE_LANG_COMPILER(Intel, C, __INTEL_COMPILER, INTELCC=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_INTELCXX],
[
    _AC_BAKEFILE_LANG_COMPILER(Intel, C++, __INTEL_COMPILER, INTELCXX=yes)
])

dnl Intel compiler command line options changed in incompatible ways sometimes
dnl before v8 (-KPIC was replaced with gcc-compatible -fPIC) and again in v10
dnl (-create-pch deprecated in favour of -pch-create) so we need to test for
dnl its exact version too
AC_DEFUN([AC_BAKEFILE_PROG_INTELCC_8],
[
    _AC_BAKEFILE_LANG_COMPILER_LATER_THAN(Intel, C, __INTEL_COMPILER, 800, 8, INTELCC8=yes)
])
AC_DEFUN([AC_BAKEFILE_PROG_INTELCXX_8],
[
    _AC_BAKEFILE_LANG_COMPILER_LATER_THAN(Intel, C++, __INTEL_COMPILER, 800, 8, INTELCXX8=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_INTELCC_10],
[
    _AC_BAKEFILE_LANG_COMPILER_LATER_THAN(Intel, C, __INTEL_COMPILER, 1000, 10, INTELCC10=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_INTELCXX_10],
[
    _AC_BAKEFILE_LANG_COMPILER_LATER_THAN(Intel, C++, __INTEL_COMPILER, 1000, 10, INTELCXX10=yes)
])

dnl HP-UX aCC: see http://docs.hp.com/en/6162/preprocess.htm#macropredef
AC_DEFUN([AC_BAKEFILE_PROG_HPCC],
[
    _AC_BAKEFILE_LANG_COMPILER(HP, C, __HP_cc, HPCC=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_HPCXX],
[
    _AC_BAKEFILE_LANG_COMPILER(HP, C++, __HP_aCC, HPCXX=yes)
])

dnl Tru64 cc and cxx
AC_DEFUN([AC_BAKEFILE_PROG_COMPAQCC],
[
    _AC_BAKEFILE_LANG_COMPILER(Compaq, C, __DECC, COMPAQCC=yes)
])

AC_DEFUN([AC_BAKEFILE_PROG_COMPAQCXX],
[
    _AC_BAKEFILE_LANG_COMPILER(Compaq, C++, __DECCXX, COMPAQCXX=yes)
])

dnl ===========================================================================
dnl macros to detect specialty compiler options
dnl ===========================================================================

dnl Figure out if we need to pass -ext o to compiler (MetroWerks)
AC_DEFUN([AC_BAKEFILE_METROWERKS_EXTO],
[AC_CACHE_CHECK([if the _AC_LANG compiler requires -ext o], bakefile_cv_[]_AC_LANG_ABBREV[]_exto,
dnl First create an empty conf test
[AC_LANG_CONFTEST([AC_LANG_PROGRAM()])
dnl Now remove .o and .c.o or .cc.o
rm -f conftest.$ac_objext conftest.$ac_ext.o
dnl Now compile the test
AS_IF([AC_TRY_EVAL(ac_compile)],
dnl If the test succeeded look for conftest.c.o or conftest.cc.o
[for ac_file in `(ls conftest.* 2>/dev/null)`; do
    case $ac_file in
        conftest.$ac_ext.o)
            bakefile_cv_[]_AC_LANG_ABBREV[]_exto="-ext o"
            ;;
        *)
            ;;
    esac
done],
[AC_MSG_FAILURE([cannot figure out if compiler needs -ext o: cannot compile])
]) dnl AS_IF

rm -f conftest.$ac_ext.o conftest.$ac_objext conftest.$ac_ext
]) dnl AC_CACHE_CHECK

if test "x$bakefile_cv_[]_AC_LANG_ABBREV[]_exto" '!=' "x"; then
    if test "[]_AC_LANG_ABBREV[]" = "c"; then
        CFLAGS="$bakefile_cv_[]_AC_LANG_ABBREV[]_exto $CFLAGS"
    fi
    if test "[]_AC_LANG_ABBREV[]" = "cxx"; then
        CXXFLAGS="$bakefile_cv_[]_AC_LANG_ABBREV[]_exto $CXXFLAGS"
    fi
fi
]) dnl AC_DEFUN


dnl ===========================================================================
dnl Macros to do all of the compiler detections as one macro
dnl ===========================================================================

dnl check for different proprietary compilers depending on target platform
dnl _AC_BAKEFILE_PROG_COMPILER(LANG)
AC_DEFUN([_AC_BAKEFILE_PROG_COMPILER],
[
    AC_REQUIRE([AC_PROG_$1])

    dnl Intel compiler can be used under several different OS and even
    dnl different architectures (x86, amd64 and Itanium) so it's easier to just
    dnl always test for it
    AC_BAKEFILE_PROG_INTEL$1

    dnl If we use Intel compiler we also need to know its version
    if test "$INTEL$1" = "yes"; then
        AC_BAKEFILE_PROG_INTEL$1_8
        AC_BAKEFILE_PROG_INTEL$1_10
    fi

    dnl if we're using gcc, we can't be using any of incompatible compilers
    if test "x$G$1" != "xyes"; then
        if test "x$1" = "xC"; then
            AC_BAKEFILE_METROWERKS_EXTO
            if test "x$bakefile_cv_c_exto" '!=' "x"; then
                unset ac_cv_prog_cc_g
                _AC_PROG_CC_G
            fi
        fi

        dnl most of these compilers are only used under well-defined OS so
        dnl don't waste time checking for them on other ones
        case `uname -s` in
            AIX*)
                AC_BAKEFILE_PROG_XL$1
                ;;

            Darwin)
                AC_BAKEFILE_PROG_MW$1
                if test "$MW$1" != "yes"; then
                    AC_BAKEFILE_PROG_XL$1
                fi
                ;;

            IRIX*)
                AC_BAKEFILE_PROG_SGI$1
                ;;

            Linux*)
                dnl Sun CC is now available under Linux too, test for it unless
                dnl we already found that we were using a different compiler
                if test "$INTEL$1" != "yes"; then
                    AC_BAKEFILE_PROG_SUN$1
                fi
                ;;

            HP-UX*)
                AC_BAKEFILE_PROG_HP$1
                ;;

            OSF1)
                AC_BAKEFILE_PROG_COMPAQ$1
                ;;

            SunOS)
                AC_BAKEFILE_PROG_SUN$1
                ;;
        esac
    fi
])

AC_DEFUN([AC_BAKEFILE_PROG_CC],
[
    _AC_BAKEFILE_PROG_COMPILER(CC)
])

AC_DEFUN([AC_BAKEFILE_PROG_CXX],
[
    _AC_BAKEFILE_PROG_COMPILER(CXX)
])


dnl
dnl  This file is part of Bakefile (http://www.bakefile.org)
dnl
dnl  Copyright (C) 2003-2007 Vaclav Slavik and others
dnl
dnl  Permission is hereby granted, free of charge, to any person obtaining a
dnl  copy of this software and associated documentation files (the "Software"),
dnl  to deal in the Software without restriction, including without limitation
dnl  the rights to use, copy, modify, merge, publish, distribute, sublicense,
dnl  and/or sell copies of the Software, and to permit persons to whom the
dnl  Software is furnished to do so, subject to the following conditions:
dnl
dnl  The above copyright notice and this permission notice shall be included in
dnl  all copies or substantial portions of the Software.
dnl
dnl  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
dnl  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
dnl  DEALINGS IN THE SOFTWARE.
dnl
dnl  $Id: bakefile.m4 1346 2011-02-01 14:03:00Z vaclavslavik $
dnl
dnl  Support macros for makefiles generated by BAKEFILE.
dnl


dnl ---------------------------------------------------------------------------
dnl Lots of compiler & linker detection code contained here was taken from
dnl wxWidgets configure.in script (see http://www.wxwidgets.org)
dnl ---------------------------------------------------------------------------



dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_GNUMAKE
dnl
dnl Detects GNU make
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_GNUMAKE],
[
    dnl does make support "-include" (only GNU make does AFAIK)?
    AC_CACHE_CHECK([if make is GNU make], bakefile_cv_prog_makeisgnu,
    [
        if ( ${SHELL-sh} -c "${MAKE-make} --version" 2> /dev/null |
                egrep -s GNU > /dev/null); then
            bakefile_cv_prog_makeisgnu="yes"
        else
            bakefile_cv_prog_makeisgnu="no"
        fi
    ])

    if test "x$bakefile_cv_prog_makeisgnu" = "xyes"; then
        IF_GNU_MAKE=""
    else
        IF_GNU_MAKE="#"
    fi
    AC_SUBST(IF_GNU_MAKE)
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_PLATFORM
dnl
dnl Detects platform and sets PLATFORM_XXX variables accordingly
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_PLATFORM],
[
    PLATFORM_UNIX=0
    PLATFORM_WIN32=0
    PLATFORM_MSDOS=0
    PLATFORM_MAC=0
    PLATFORM_MACOS=0
    PLATFORM_MACOSX=0
    PLATFORM_OS2=0
    PLATFORM_BEOS=0

    if test "x$BAKEFILE_FORCE_PLATFORM" = "x"; then
        case "${BAKEFILE_HOST}" in
            *-*-mingw32* )
                PLATFORM_WIN32=1
            ;;
            *-pc-msdosdjgpp )
                PLATFORM_MSDOS=1
            ;;
            *-pc-os2_emx | *-pc-os2-emx )
                PLATFORM_OS2=1
            ;;
            *-*-darwin* )
                PLATFORM_MAC=1
                PLATFORM_MACOSX=1
            ;;
            *-*-beos* )
                PLATFORM_BEOS=1
            ;;
            powerpc-apple-macos* )
                PLATFORM_MAC=1
                PLATFORM_MACOS=1
            ;;
            * )
                PLATFORM_UNIX=1
            ;;
        esac
    else
        case "$BAKEFILE_FORCE_PLATFORM" in
            win32 )
                PLATFORM_WIN32=1
            ;;
            msdos )
                PLATFORM_MSDOS=1
            ;;
            os2 )
                PLATFORM_OS2=1
            ;;
            darwin )
                PLATFORM_MAC=1
                PLATFORM_MACOSX=1
            ;;
            unix )
                PLATFORM_UNIX=1
            ;;
            beos )
                PLATFORM_BEOS=1
            ;;
            * )
                AC_MSG_ERROR([Unknown platform: $BAKEFILE_FORCE_PLATFORM])
            ;;
        esac
    fi

    AC_SUBST(PLATFORM_UNIX)
    AC_SUBST(PLATFORM_WIN32)
    AC_SUBST(PLATFORM_MSDOS)
    AC_SUBST(PLATFORM_MAC)
    AC_SUBST(PLATFORM_MACOS)
    AC_SUBST(PLATFORM_MACOSX)
    AC_SUBST(PLATFORM_OS2)
    AC_SUBST(PLATFORM_BEOS)
])


dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_PLATFORM_SPECIFICS
dnl
dnl Sets misc platform-specific settings
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_PLATFORM_SPECIFICS],
[
    AC_ARG_ENABLE([omf], AS_HELP_STRING([--enable-omf],
                                        [use OMF object format (OS/2)]),
                  [bk_os2_use_omf="$enableval"])

    case "${BAKEFILE_HOST}" in
      *-*-darwin* )
        dnl For Unix to MacOS X porting instructions, see:
        dnl http://fink.sourceforge.net/doc/porting/porting.html
        if test "x$GCC" = "xyes"; then
            CFLAGS="$CFLAGS -fno-common"
            CXXFLAGS="$CXXFLAGS -fno-common"
        fi
        if test "x$XLCC" = "xyes"; then
            CFLAGS="$CFLAGS -qnocommon"
            CXXFLAGS="$CXXFLAGS -qnocommon"
        fi
        ;;

      *-pc-os2_emx | *-pc-os2-emx )
        if test "x$bk_os2_use_omf" = "xyes" ; then
            AR=emxomfar
            RANLIB=:
            LDFLAGS="-Zomf $LDFLAGS"
            CFLAGS="-Zomf $CFLAGS"
            CXXFLAGS="-Zomf $CXXFLAGS"
            OS2_LIBEXT="lib"
        else
            OS2_LIBEXT="a"
        fi
        ;;

      i*86-*-beos* )
        LDFLAGS="-L/boot/develop/lib/x86 $LDFLAGS"
        ;;
    esac
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_SUFFIXES
dnl
dnl Detects shared various suffixes for shared libraries, libraries, programs,
dnl plugins etc.
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_SUFFIXES],
[
    SO_SUFFIX="so"
    SO_SUFFIX_MODULE="so"
    EXEEXT=""
    LIBPREFIX="lib"
    LIBEXT=".a"
    DLLPREFIX="lib"
    DLLPREFIX_MODULE=""
    DLLIMP_SUFFIX=""
    dlldir="$libdir"

    case "${BAKEFILE_HOST}" in
        dnl PA-RISC HP systems used .sl but IA64 use ELF-64 and so use the
        dnl standard .so extension
        ia64-hp-hpux* )
        ;;
        *-hp-hpux* )
            SO_SUFFIX="sl"
            SO_SUFFIX_MODULE="sl"
        ;;
        *-*-aix* )
            dnl quoting from
            dnl http://www-1.ibm.com/servers/esdd/articles/gnu.html:
            dnl     Both archive libraries and shared libraries on AIX have an
            dnl     .a extension. This will explain why you can't link with an
            dnl     .so and why it works with the name changed to .a.
            SO_SUFFIX="a"
            SO_SUFFIX_MODULE="a"
        ;;
        *-*-cygwin* )
            SO_SUFFIX="dll"
            SO_SUFFIX_MODULE="dll"
            DLLIMP_SUFFIX="dll.a"
            EXEEXT=".exe"
            DLLPREFIX="cyg"
            dlldir="$bindir"
        ;;
        *-*-mingw32* )
            SO_SUFFIX="dll"
            SO_SUFFIX_MODULE="dll"
            DLLIMP_SUFFIX="dll.a"
            EXEEXT=".exe"
            DLLPREFIX=""
            dlldir="$bindir"
        ;;
        *-pc-msdosdjgpp )
            EXEEXT=".exe"
            DLLPREFIX=""
            dlldir="$bindir"
        ;;
        *-pc-os2_emx | *-pc-os2-emx )
            SO_SUFFIX="dll"
            SO_SUFFIX_MODULE="dll"
            DLLIMP_SUFFIX=$OS2_LIBEXT
            EXEEXT=".exe"
            DLLPREFIX=""
            LIBPREFIX=""
            LIBEXT=".$OS2_LIBEXT"
            dlldir="$bindir"
        ;;
        *-*-darwin* )
            SO_SUFFIX="dylib"
            SO_SUFFIX_MODULE="bundle"
        ;;
    esac

    if test "x$DLLIMP_SUFFIX" = "x" ; then
        DLLIMP_SUFFIX="$SO_SUFFIX"
    fi

    AC_SUBST(SO_SUFFIX)
    AC_SUBST(SO_SUFFIX_MODULE)
    AC_SUBST(DLLIMP_SUFFIX)
    AC_SUBST(EXEEXT)
    AC_SUBST(LIBPREFIX)
    AC_SUBST(LIBEXT)
    AC_SUBST(DLLPREFIX)
    AC_SUBST(DLLPREFIX_MODULE)
    AC_SUBST(dlldir)
])


dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_SHARED_LD
dnl
dnl Detects command for making shared libraries, substitutes SHARED_LD_CC
dnl and SHARED_LD_CXX.
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_SHARED_LD],
[
    dnl the extra compiler flags needed for compilation of shared library
    PIC_FLAG=""
    if test "x$GCC" = "xyes"; then
        dnl the switch for gcc is the same under all platforms
        PIC_FLAG="-fPIC"
    fi

    dnl Defaults for GCC and ELF .so shared libs:
    SHARED_LD_CC="\$(CC) -shared ${PIC_FLAG} -o"
    SHARED_LD_CXX="\$(CXX) -shared ${PIC_FLAG} -o"
    WINDOWS_IMPLIB=0

    case "${BAKEFILE_HOST}" in
      *-hp-hpux* )
        dnl default settings are good for gcc but not for the native HP-UX
        if test "x$GCC" != "xyes"; then
            dnl no idea why it wants it, but it does
            LDFLAGS="$LDFLAGS -L/usr/lib"

            SHARED_LD_CC="${CC} -b -o"
            SHARED_LD_CXX="${CXX} -b -o"
            PIC_FLAG="+Z"
        fi
      ;;

      *-*-linux* )
        dnl newer icc versions use -fPIC just as gcc does and, in fact, the
        dnl newest (v10+) ones don't even understand -KPIC any longer
        if test "$INTELCC" = "yes" -a "$INTELCC8" != "yes"; then
            PIC_FLAG="-KPIC"
        elif test "x$SUNCXX" = "xyes"; then
            SHARED_LD_CC="${CC} -G -o"
            SHARED_LD_CXX="${CXX} -G -o"
            PIC_FLAG="-KPIC"
        fi
      ;;

      *-*-solaris2* )
        if test "x$SUNCXX" = xyes ; then
            SHARED_LD_CC="${CC} -G -o"
            SHARED_LD_CXX="${CXX} -G -o"
            PIC_FLAG="-KPIC"
        fi
      ;;

      *-*-darwin* )
        AC_BAKEFILE_CREATE_FILE_SHARED_LD_SH
        chmod +x shared-ld-sh

        SHARED_LD_MODULE_CC="`pwd`/shared-ld-sh -bundle -headerpad_max_install_names -o"
        SHARED_LD_MODULE_CXX="CXX=\"\$(CXX)\" $SHARED_LD_MODULE_CC"

        dnl Most apps benefit from being fully binded (its faster and static
        dnl variables initialized at startup work).
        dnl This can be done either with the exe linker flag -Wl,-bind_at_load
        dnl or with a double stage link in order to create a single module
        dnl "-init _wxWindowsDylibInit" not useful with lazy linking solved

        dnl If using newer dev tools then there is a -single_module flag that
        dnl we can use to do this for dylibs, otherwise we'll need to use a helper
        dnl script.  Check the version of gcc to see which way we can go:
        AC_CACHE_CHECK([for gcc 3.1 or later], bakefile_cv_gcc31, [
           AC_TRY_COMPILE([],
               [
                   #if (__GNUC__ < 3) || \
                       ((__GNUC__ == 3) && (__GNUC_MINOR__ < 1))
                       This is old gcc
                   #endif
               ],
               [
                   bakefile_cv_gcc31=yes
               ],
               [
                   bakefile_cv_gcc31=no
               ]
           )
        ])
        if test "$bakefile_cv_gcc31" = "no"; then
            dnl Use the shared-ld-sh helper script
            SHARED_LD_CC="`pwd`/shared-ld-sh -dynamiclib -headerpad_max_install_names -o"
            SHARED_LD_CXX="$SHARED_LD_CC"
        else
            dnl Use the -single_module flag and let the linker do it for us
            SHARED_LD_CC="\${CC} -dynamiclib -single_module -headerpad_max_install_names -o"
            SHARED_LD_CXX="\${CXX} -dynamiclib -single_module -headerpad_max_install_names -o"
        fi

        if test "x$GCC" == "xyes"; then
            PIC_FLAG="-dynamic -fPIC"
        fi
        if test "x$XLCC" = "xyes"; then
            PIC_FLAG="-dynamic -DPIC"
        fi
      ;;

      *-*-aix* )
        if test "x$GCC" = "xyes"; then
            dnl at least gcc 2.95 warns that -fPIC is ignored when
            dnl compiling each and every file under AIX which is annoying,
            dnl so don't use it there (it's useless as AIX runs on
            dnl position-independent architectures only anyhow)
            PIC_FLAG=""

            dnl -bexpfull is needed by AIX linker to export all symbols (by
            dnl default it doesn't export any and even with -bexpall it
            dnl doesn't export all C++ support symbols, e.g. vtable
            dnl pointers) but it's only available starting from 5.1 (with
            dnl maintenance pack 2, whatever this is), see
            dnl http://www-128.ibm.com/developerworks/eserver/articles/gnu.html
            case "${BAKEFILE_HOST}" in
                *-*-aix5* )
                    LD_EXPFULL="-Wl,-bexpfull"
                    ;;
            esac

            SHARED_LD_CC="\$(CC) -shared $LD_EXPFULL -o"
            SHARED_LD_CXX="\$(CXX) -shared $LD_EXPFULL -o"
        else
            dnl FIXME: makeC++SharedLib is obsolete, what should we do for
            dnl        recent AIX versions?
            AC_CHECK_PROG(AIX_CXX_LD, makeC++SharedLib,
                          makeC++SharedLib, /usr/lpp/xlC/bin/makeC++SharedLib)
            SHARED_LD_CC="$AIX_CC_LD -p 0 -o"
            SHARED_LD_CXX="$AIX_CXX_LD -p 0 -o"
        fi
      ;;

      *-*-beos* )
        dnl can't use gcc under BeOS for shared library creation because it
        dnl complains about missing 'main'
        SHARED_LD_CC="${LD} -nostart -o"
        SHARED_LD_CXX="${LD} -nostart -o"
      ;;

      *-*-irix* )
        dnl default settings are ok for gcc
        if test "x$GCC" != "xyes"; then
            PIC_FLAG="-KPIC"
        fi
      ;;

      *-*-cygwin* | *-*-mingw32* )
        PIC_FLAG=""
        SHARED_LD_CC="\$(CC) -shared -o"
        SHARED_LD_CXX="\$(CXX) -shared -o"
        WINDOWS_IMPLIB=1
      ;;

      *-pc-os2_emx | *-pc-os2-emx )
        SHARED_LD_CC="`pwd`/dllar.sh -libf INITINSTANCE -libf TERMINSTANCE -o"
        SHARED_LD_CXX="`pwd`/dllar.sh -libf INITINSTANCE -libf TERMINSTANCE -o"
        PIC_FLAG=""
        AC_BAKEFILE_CREATE_FILE_DLLAR_SH
        chmod +x dllar.sh
      ;;

      powerpc-apple-macos* | \
      *-*-freebsd* | *-*-openbsd* | *-*-netbsd* | *-*-k*bsd*-gnu | \
      *-*-mirbsd* | \
      *-*-sunos4* | \
      *-*-osf* | \
      *-*-dgux5* | \
      *-*-sysv5* | \
      *-pc-msdosdjgpp )
        dnl defaults are ok
      ;;

      *)
        AC_MSG_ERROR(unknown system type $BAKEFILE_HOST.)
    esac

    if test "x$PIC_FLAG" != "x" ; then
        PIC_FLAG="$PIC_FLAG -DPIC"
    fi

    if test "x$SHARED_LD_MODULE_CC" = "x" ; then
        SHARED_LD_MODULE_CC="$SHARED_LD_CC"
    fi
    if test "x$SHARED_LD_MODULE_CXX" = "x" ; then
        SHARED_LD_MODULE_CXX="$SHARED_LD_CXX"
    fi

    AC_SUBST(SHARED_LD_CC)
    AC_SUBST(SHARED_LD_CXX)
    AC_SUBST(SHARED_LD_MODULE_CC)
    AC_SUBST(SHARED_LD_MODULE_CXX)
    AC_SUBST(PIC_FLAG)
    AC_SUBST(WINDOWS_IMPLIB)
])


dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_SHARED_VERSIONS
dnl
dnl Detects linker options for attaching versions (sonames) to shared  libs.
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_SHARED_VERSIONS],
[
    USE_SOVERSION=0
    USE_SOVERLINUX=0
    USE_SOVERSOLARIS=0
    USE_SOVERCYGWIN=0
    USE_SOTWOSYMLINKS=0
    USE_MACVERSION=0
    SONAME_FLAG=

    case "${BAKEFILE_HOST}" in
      *-*-linux* | *-*-freebsd* | *-*-openbsd* | *-*-netbsd* | \
      *-*-k*bsd*-gnu | *-*-mirbsd* )
        if test "x$SUNCXX" = "xyes"; then
            SONAME_FLAG="-h "
        else
            SONAME_FLAG="-Wl,-soname,"
        fi
        USE_SOVERSION=1
        USE_SOVERLINUX=1
        USE_SOTWOSYMLINKS=1
      ;;

      *-*-solaris2* )
        SONAME_FLAG="-h "
        USE_SOVERSION=1
        USE_SOVERSOLARIS=1
      ;;

      *-*-darwin* )
        USE_MACVERSION=1
        USE_SOVERSION=1
        USE_SOTWOSYMLINKS=1
      ;;

      *-*-cygwin* )
        USE_SOVERSION=1
        USE_SOVERCYGWIN=1
      ;;
    esac

    AC_SUBST(USE_SOVERSION)
    AC_SUBST(USE_SOVERLINUX)
    AC_SUBST(USE_SOVERSOLARIS)
    AC_SUBST(USE_SOVERCYGWIN)
    AC_SUBST(USE_MACVERSION)
    AC_SUBST(USE_SOTWOSYMLINKS)
    AC_SUBST(SONAME_FLAG)
])


dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_DEPS
dnl
dnl Detects available C/C++ dependency tracking options
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_DEPS],
[
    AC_ARG_ENABLE([dependency-tracking],
                  AS_HELP_STRING([--disable-dependency-tracking],
                                 [don't use dependency tracking even if the compiler can]),
                  [bk_use_trackdeps="$enableval"])

    AC_MSG_CHECKING([for dependency tracking method])

    BK_DEPS=""
    if test "x$bk_use_trackdeps" = "xno" ; then
        DEPS_TRACKING=0
        AC_MSG_RESULT([disabled])
    else
        DEPS_TRACKING=1

        if test "x$GCC" = "xyes"; then
            DEPSMODE=gcc
            case "${BAKEFILE_HOST}" in
                *-*-darwin* )
                    dnl -cpp-precomp (the default) conflicts with -MMD option
                    dnl used by bk-deps (see also http://developer.apple.com/documentation/Darwin/Conceptual/PortingUnix/compiling/chapter_4_section_3.html)
                    DEPSFLAG="-no-cpp-precomp -MMD"
                ;;
                * )
                    DEPSFLAG="-MMD"
                ;;
            esac
            AC_MSG_RESULT([gcc])
        elif test "x$MWCC" = "xyes"; then
            DEPSMODE=mwcc
            DEPSFLAG="-MM"
            AC_MSG_RESULT([mwcc])
        elif test "x$SUNCC" = "xyes"; then
            DEPSMODE=unixcc
            DEPSFLAG="-xM1"
            AC_MSG_RESULT([Sun cc])
        elif test "x$SGICC" = "xyes"; then
            DEPSMODE=unixcc
            DEPSFLAG="-M"
            AC_MSG_RESULT([SGI cc])
        elif test "x$HPCC" = "xyes"; then
            DEPSMODE=unixcc
            DEPSFLAG="+make"
            AC_MSG_RESULT([HP cc])
        elif test "x$COMPAQCC" = "xyes"; then
            DEPSMODE=gcc
            DEPSFLAG="-MD"
            AC_MSG_RESULT([Compaq cc])
        else
            DEPS_TRACKING=0
            AC_MSG_RESULT([none])
        fi

        if test $DEPS_TRACKING = 1 ; then
            AC_BAKEFILE_CREATE_FILE_BK_DEPS
            chmod +x bk-deps
            dnl FIXME: make this $(top_builddir)/bk-deps once autoconf-2.60
            dnl        is required (and so top_builddir is never empty):
            BK_DEPS="`pwd`/bk-deps"
        fi
    fi

    AC_SUBST(DEPS_TRACKING)
    AC_SUBST(BK_DEPS)
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_CHECK_BASIC_STUFF
dnl
dnl Checks for presence of basic programs, such as C and C++ compiler, "ranlib"
dnl or "install"
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_CHECK_BASIC_STUFF],
[
    AC_PROG_RANLIB
    AC_PROG_INSTALL
    AC_PROG_LN_S

    AC_PROG_MAKE_SET
    AC_SUBST(MAKE_SET)

    if test "x$SUNCXX" = "xyes"; then
        dnl Sun C++ compiler requires special way of creating static libs;
        dnl see here for more details:
        dnl https://sourceforge.net/tracker/?func=detail&atid=109863&aid=1229751&group_id=9863
        AR=$CXX
        AROPTIONS="-xar -o"
        AC_SUBST(AR)
    elif test "x$SGICC" = "xyes"; then
        dnl Almost the same as above for SGI mipsPro compiler
        AR=$CXX
        AROPTIONS="-ar -o"
        AC_SUBST(AR)
    else
        AC_CHECK_TOOL(AR, ar, ar)
        AROPTIONS=rcu
    fi
    AC_SUBST(AROPTIONS)

    AC_CHECK_TOOL(STRIP, strip, :)
    AC_CHECK_TOOL(NM, nm, :)

    dnl This check is necessary because "install -d" doesn't exist on
    dnl all platforms (e.g. HP/UX), see http://www.bakefile.org/ticket/80
    AC_MSG_CHECKING([for command to install directories])
    INSTALL_TEST_DIR=acbftest$$
    $INSTALL -d $INSTALL_TEST_DIR > /dev/null 2>&1
    if test $? = 0 -a -d $INSTALL_TEST_DIR; then
        rmdir $INSTALL_TEST_DIR
        dnl we must refer to makefile's $(INSTALL) variable and not
        dnl current value of shell variable, hence the single quoting:
        INSTALL_DIR='$(INSTALL) -d'
        AC_MSG_RESULT([$INSTALL -d])
    else
        INSTALL_DIR="mkdir -p"
        AC_MSG_RESULT([mkdir -p])
    fi
    AC_SUBST(INSTALL_DIR)

    LDFLAGS_GUI=
    case ${BAKEFILE_HOST} in
        *-*-cygwin* | *-*-mingw32* )
        LDFLAGS_GUI="-mwindows"
    esac
    AC_SUBST(LDFLAGS_GUI)
])


dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_RES_COMPILERS
dnl
dnl Checks for presence of resource compilers for win32 or mac
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_RES_COMPILERS],
[
    case ${BAKEFILE_HOST} in
        *-*-cygwin* | *-*-mingw32* )
            dnl Check for win32 resources compiler:
            AC_CHECK_TOOL(WINDRES, windres)
         ;;

      *-*-darwin* | powerpc-apple-macos* )
            AC_CHECK_PROG(REZ, Rez, Rez, /Developer/Tools/Rez)
            AC_CHECK_PROG(SETFILE, SetFile, SetFile, /Developer/Tools/SetFile)
        ;;
    esac

    AC_SUBST(WINDRES)
    AC_SUBST(REZ)
    AC_SUBST(SETFILE)
])

dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE_PRECOMP_HEADERS
dnl
dnl Check for precompiled headers support (GCC >= 3.4)
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_PRECOMP_HEADERS],
[

    AC_ARG_ENABLE([precomp-headers],
                  AS_HELP_STRING([--disable-precomp-headers],
                                 [don't use precompiled headers even if compiler can]),
                  [bk_use_pch="$enableval"])

    GCC_PCH=0
    ICC_PCH=0
    USE_PCH=0
    BK_MAKE_PCH=""

    case ${BAKEFILE_HOST} in
        *-*-cygwin* )
            dnl PCH support is broken in cygwin gcc because of unportable
            dnl assumptions about mmap() in gcc code which make PCH generation
            dnl fail erratically; disable PCH completely until this is fixed
            bk_use_pch="no"
            ;;
    esac

    if test "x$bk_use_pch" = "x" -o "x$bk_use_pch" = "xyes" ; then
        if test "x$GCC" = "xyes"; then
            dnl test if we have gcc-3.4:
            AC_MSG_CHECKING([if the compiler supports precompiled headers])
            AC_TRY_COMPILE([],
                [
                    #if !defined(__GNUC__) || !defined(__GNUC_MINOR__)
                        There is no PCH support
                    #endif
                    #if (__GNUC__ < 3)
                        There is no PCH support
                    #endif
                    #if (__GNUC__ == 3) && \
                       ((!defined(__APPLE_CC__) && (__GNUC_MINOR__ < 4)) || \
                       ( defined(__APPLE_CC__) && (__GNUC_MINOR__ < 3))) || \
                       ( defined(__INTEL_COMPILER) )
                        There is no PCH support
                    #endif
                ],
                [
                    AC_MSG_RESULT([yes])
                    GCC_PCH=1
                ],
                [
                    if test "$INTELCXX8" = "yes"; then
                        AC_MSG_RESULT([yes])
                        ICC_PCH=1
                        if test "$INTELCXX10" = "yes"; then
                            ICC_PCH_CREATE_SWITCH="-pch-create"
                            ICC_PCH_USE_SWITCH="-pch-use"
                        else
                            ICC_PCH_CREATE_SWITCH="-create-pch"
                            ICC_PCH_USE_SWITCH="-use-pch"
                        fi
                    else
                        AC_MSG_RESULT([no])
                    fi
                ])
            if test $GCC_PCH = 1 -o $ICC_PCH = 1 ; then
                USE_PCH=1
                AC_BAKEFILE_CREATE_FILE_BK_MAKE_PCH
                chmod +x bk-make-pch
                dnl FIXME: make this $(top_builddir)/bk-make-pch once
                dnl        autoconf-2.60 is required (and so top_builddir is
                dnl        never empty):
                BK_MAKE_PCH="`pwd`/bk-make-pch"
            fi
        fi
    fi

    AC_SUBST(GCC_PCH)
    AC_SUBST(ICC_PCH)
    AC_SUBST(ICC_PCH_CREATE_SWITCH)
    AC_SUBST(ICC_PCH_USE_SWITCH)
    AC_SUBST(BK_MAKE_PCH)
])



dnl ---------------------------------------------------------------------------
dnl AC_BAKEFILE([autoconf_inc.m4 inclusion])
dnl
dnl To be used in configure.in of any project using Bakefile-generated mks
dnl
dnl Behaviour can be modified by setting following variables:
dnl    BAKEFILE_CHECK_BASICS    set to "no" if you don't want bakefile to
dnl                             to perform check for basic tools like ranlib
dnl    BAKEFILE_HOST            set this to override host detection, defaults
dnl                             to ${host}
dnl    BAKEFILE_FORCE_PLATFORM  set to override platform detection
dnl
dnl Example usage:
dnl
dnl   AC_BAKEFILE([FOO(autoconf_inc.m4)])
dnl
dnl (replace FOO with m4_include above, aclocal would die otherwise)
dnl (yes, it's ugly, but thanks to a bug in aclocal, it's the only thing
dnl we can do...)
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE],
[
    AC_PREREQ([2.58])

    dnl We need to always run C/C++ compiler tests, but it's also possible
    dnl for the user to call these macros manually, hence this instead of
    dnl simply calling these macros. See http://www.bakefile.org/ticket/64
    AC_REQUIRE([AC_BAKEFILE_PROG_CC])
    AC_REQUIRE([AC_BAKEFILE_PROG_CXX])

    if test "x$BAKEFILE_HOST" = "x"; then
               if test "x${host}" = "x" ; then
                       AC_MSG_ERROR([You must call the autoconf "CANONICAL_HOST" macro in your configure.ac (or .in) file.])
               fi

        BAKEFILE_HOST="${host}"
    fi

    if test "x$BAKEFILE_CHECK_BASICS" != "xno"; then
        AC_BAKEFILE_CHECK_BASIC_STUFF
    fi
    AC_BAKEFILE_GNUMAKE
    AC_BAKEFILE_PLATFORM
    AC_BAKEFILE_PLATFORM_SPECIFICS
    AC_BAKEFILE_SUFFIXES
    AC_BAKEFILE_SHARED_LD
    AC_BAKEFILE_SHARED_VERSIONS
    AC_BAKEFILE_DEPS
    AC_BAKEFILE_RES_COMPILERS

    dnl OBJCFLAGS is set by Autoconf, but OBJCXXFLAGS is not:
    AC_SUBST(OBJCXXFLAGS)


    BAKEFILE_BAKEFILE_M4_VERSION="0.2.9"

    dnl includes autoconf_inc.m4:
    $1

    if test "$BAKEFILE_AUTOCONF_INC_M4_VERSION" = "" ; then
        AC_MSG_ERROR([No version found in autoconf_inc.m4 - bakefile macro was changed to take additional argument, perhaps configure.in wasn't updated (see the documentation)?])
    fi

    if test "$BAKEFILE_BAKEFILE_M4_VERSION" != "$BAKEFILE_AUTOCONF_INC_M4_VERSION" ; then
        AC_MSG_ERROR([Versions of Bakefile used to generate makefiles ($BAKEFILE_AUTOCONF_INC_M4_VERSION) and configure ($BAKEFILE_BAKEFILE_M4_VERSION) do not match.])
    fi
])


dnl ---------------------------------------------------------------------------
dnl              Embedded copies of helper scripts follow:
dnl ---------------------------------------------------------------------------

AC_DEFUN([AC_BAKEFILE_CREATE_FILE_BK_DEPS],
[
dnl ===================== bk-deps begins here =====================
dnl    (Created by merge-scripts.py from bk-deps
dnl     file do not edit here!)
D='$'
cat <<EOF >bk-deps
#!/bin/sh

# This script is part of Bakefile (http://www.bakefile.org) autoconf
# script. It is used to track C/C++ files dependencies in portable way.
#
# Permission is given to use this file in any way.

DEPSMODE=${DEPSMODE}
DEPSFLAG="${DEPSFLAG}"
DEPSDIRBASE=.deps

if test ${D}DEPSMODE = gcc ; then
    ${D}* ${D}{DEPSFLAG}
    status=${D}?

    # determine location of created files:
    while test ${D}# -gt 0; do
        case "${D}1" in
            -o )
                shift
                objfile=${D}1
            ;;
            -* )
            ;;
            * )
                srcfile=${D}1
            ;;
        esac
        shift
    done
    objfilebase=\`basename ${D}objfile\`
    builddir=\`dirname ${D}objfile\`
    depfile=\`basename ${D}srcfile | sed -e 's/\\..*${D}/.d/g'\`
    depobjname=\`echo ${D}depfile |sed -e 's/\\.d/.o/g'\`
    depsdir=${D}builddir/${D}DEPSDIRBASE
    mkdir -p ${D}depsdir

    # if the compiler failed, we're done:
    if test ${D}{status} != 0 ; then
        rm -f ${D}depfile
        exit ${D}{status}
    fi

    # move created file to the location we want it in:
    if test -f ${D}depfile ; then
        sed -e "s,${D}depobjname:,${D}objfile:,g" ${D}depfile >${D}{depsdir}/${D}{objfilebase}.d
        rm -f ${D}depfile
    else
        # "g++ -MMD -o fooobj.o foosrc.cpp" produces fooobj.d
        depfile=\`echo "${D}objfile" | sed -e 's/\\..*${D}/.d/g'\`
        if test ! -f ${D}depfile ; then
            # "cxx -MD -o fooobj.o foosrc.cpp" creates fooobj.o.d (Compaq C++)
            depfile="${D}objfile.d"
        fi
        if test -f ${D}depfile ; then
            sed -e "\\,^${D}objfile,!s,${D}depobjname:,${D}objfile:,g" ${D}depfile >${D}{depsdir}/${D}{objfilebase}.d
            rm -f ${D}depfile
        fi
    fi
    exit 0

elif test ${D}DEPSMODE = mwcc ; then
    ${D}* || exit ${D}?
    # Run mwcc again with -MM and redirect into the dep file we want
    # NOTE: We can't use shift here because we need ${D}* to be valid
    prevarg=
    for arg in ${D}* ; do
        if test "${D}prevarg" = "-o"; then
            objfile=${D}arg
        else
            case "${D}arg" in
                -* )
                ;;
                * )
                    srcfile=${D}arg
                ;;
            esac
        fi
        prevarg="${D}arg"
    done

    objfilebase=\`basename ${D}objfile\`
    builddir=\`dirname ${D}objfile\`
    depsdir=${D}builddir/${D}DEPSDIRBASE
    mkdir -p ${D}depsdir

    ${D}* ${D}DEPSFLAG >${D}{depsdir}/${D}{objfilebase}.d
    exit 0

elif test ${D}DEPSMODE = unixcc; then
    ${D}* || exit ${D}?
    # Run compiler again with deps flag and redirect into the dep file.
    # It doesn't work if the '-o FILE' option is used, but without it the
    # dependency file will contain the wrong name for the object. So it is
    # removed from the command line, and the dep file is fixed with sed.
    cmd=""
    while test ${D}# -gt 0; do
        case "${D}1" in
            -o )
                shift
                objfile=${D}1
            ;;
            * )
                eval arg${D}#=\\${D}1
                cmd="${D}cmd \\${D}arg${D}#"
            ;;
        esac
        shift
    done

    objfilebase=\`basename ${D}objfile\`
    builddir=\`dirname ${D}objfile\`
    depsdir=${D}builddir/${D}DEPSDIRBASE
    mkdir -p ${D}depsdir

    eval "${D}cmd ${D}DEPSFLAG" | sed "s|.*:|${D}objfile:|" >${D}{depsdir}/${D}{objfilebase}.d
    exit 0

else
    ${D}*
    exit ${D}?
fi
EOF
dnl ===================== bk-deps ends here =====================
])

AC_DEFUN([AC_BAKEFILE_CREATE_FILE_SHARED_LD_SH],
[
dnl ===================== shared-ld-sh begins here =====================
dnl    (Created by merge-scripts.py from shared-ld-sh
dnl     file do not edit here!)
D='$'
cat <<EOF >shared-ld-sh
#!/bin/sh
#-----------------------------------------------------------------------------
#-- Name:        distrib/mac/shared-ld-sh
#-- Purpose:     Link a mach-o dynamic shared library for Darwin / Mac OS X
#-- Author:      Gilles Depeyrot
#-- Copyright:   (c) 2002 Gilles Depeyrot
#-- Licence:     any use permitted
#-----------------------------------------------------------------------------

verbose=0
args=""
objects=""
linking_flag="-dynamiclib"
ldargs="-r -keep_private_externs -nostdlib"

if test "x${D}CXX" = "x"; then
    CXX="c++"
fi

while test ${D}# -gt 0; do
    case ${D}1 in

       -v)
        verbose=1
        ;;

       -o|-compatibility_version|-current_version|-framework|-undefined|-install_name)
        # collect these options and values
        args="${D}{args} ${D}1 ${D}2"
        shift
        ;;
       
       -arch|-isysroot)
        # collect these options and values
        ldargs="${D}{ldargs} ${D}1 ${D}2"
        shift
        ;;

       -s|-Wl,*)
        # collect these load args
        ldargs="${D}{ldargs} ${D}1"
        ;;

       -l*|-L*|-flat_namespace|-headerpad_max_install_names)
        # collect these options
        args="${D}{args} ${D}1"
        ;;

       -dynamiclib|-bundle)
        linking_flag="${D}1"
        ;;

       -*)
        echo "shared-ld: unhandled option '${D}1'"
        exit 1
        ;;

        *.o | *.a | *.dylib)
        # collect object files
        objects="${D}{objects} ${D}1"
        ;;

        *)
        echo "shared-ld: unhandled argument '${D}1'"
        exit 1
        ;;

    esac
    shift
done

status=0

#
# Link one module containing all the others
#
if test ${D}{verbose} = 1; then
    echo "${D}CXX ${D}{ldargs} ${D}{objects} -o master.${D}${D}.o"
fi
${D}CXX ${D}{ldargs} ${D}{objects} -o master.${D}${D}.o
status=${D}?

#
# Link the shared library from the single module created, but only if the
# previous command didn't fail:
#
if test ${D}{status} = 0; then
    if test ${D}{verbose} = 1; then
        echo "${D}CXX ${D}{linking_flag} master.${D}${D}.o ${D}{args}"
    fi
    ${D}CXX ${D}{linking_flag} master.${D}${D}.o ${D}{args}
    status=${D}?
fi

#
# Remove intermediate module
#
rm -f master.${D}${D}.o

exit ${D}status
EOF
dnl ===================== shared-ld-sh ends here =====================
])

AC_DEFUN([AC_BAKEFILE_CREATE_FILE_BK_MAKE_PCH],
[
dnl ===================== bk-make-pch begins here =====================
dnl    (Created by merge-scripts.py from bk-make-pch
dnl     file do not edit here!)
D='$'
cat <<EOF >bk-make-pch
#!/bin/sh

# This script is part of Bakefile (http://www.bakefile.org) autoconf
# script. It is used to generated precompiled headers.
#
# Permission is given to use this file in any way.

outfile="${D}{1}"
header="${D}{2}"
shift
shift

builddir=\`echo ${D}outfile | sed -e 's,/\\.pch/.*${D},,g'\`

compiler=""
headerfile=""

while test ${D}{#} -gt 0; do
    add_to_cmdline=1
    case "${D}{1}" in
        -I* )
            incdir=\`echo ${D}{1} | sed -e 's/-I\\(.*\\)/\\1/g'\`
            if test "x${D}{headerfile}" = "x" -a -f "${D}{incdir}/${D}{header}" ; then
                headerfile="${D}{incdir}/${D}{header}"
            fi
        ;;
        -use-pch|-use_pch|-pch-use )
            shift
            add_to_cmdline=0
        ;;
    esac
    if test ${D}add_to_cmdline = 1 ; then
        compiler="${D}{compiler} ${D}{1}"
    fi
    shift
done

if test "x${D}{headerfile}" = "x" ; then
    echo "error: can't find header ${D}{header} in include paths" >&2
else
    if test -f ${D}{outfile} ; then
        rm -f ${D}{outfile}
    else
        mkdir -p \`dirname ${D}{outfile}\`
    fi
    depsfile="${D}{builddir}/.deps/\`echo ${D}{outfile} | tr '/.' '__'\`.d"
    mkdir -p ${D}{builddir}/.deps
    if test "x${GCC_PCH}" = "x1" ; then
        # can do this because gcc is >= 3.4:
        ${D}{compiler} -o ${D}{outfile} -MMD -MF "${D}{depsfile}" "${D}{headerfile}"
    elif test "x${ICC_PCH}" = "x1" ; then
        filename=pch_gen-${D}${D}
        file=${D}{filename}.c
        dfile=${D}{filename}.d
        cat > ${D}file <<EOT
#include "${D}header"
EOT
        # using -MF icc complains about differing command lines in creation/use
        ${D}compiler -c ${ICC_PCH_CREATE_SWITCH} ${D}outfile -MMD ${D}file && \\
          sed -e "s,^.*:,${D}outfile:," -e "s, ${D}file,," < ${D}dfile > ${D}depsfile && \\
          rm -f ${D}file ${D}dfile ${D}{filename}.o
    fi
    exit ${D}{?}
fi
EOF
dnl ===================== bk-make-pch ends here =====================
])

