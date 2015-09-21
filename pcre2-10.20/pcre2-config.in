#!/bin/sh

prefix=@prefix@
exec_prefix=@exec_prefix@
exec_prefix_set=no

cflags="[--cflags]"
libs=

if test @enable_pcre2_16@ = yes ; then
  libs="[--libs16] $libs"
fi

if test @enable_pcre2_32@ = yes ; then
  libs="[--libs32] $libs"
fi

if test @enable_pcre2_8@ = yes ; then
  libs="[--libs8] [--libs-posix] $libs"
  cflags="$cflags [--cflags-posix]"
fi

usage="Usage: pcre2-config [--prefix] [--exec-prefix] [--version] $libs $cflags"

if test $# -eq 0; then
      echo "${usage}" 1>&2
      exit 1
fi

libR=
case `uname -s` in
  *SunOS*)
  libR=" -R@libdir@"
  ;;
  *BSD*)
  libR=" -Wl,-R@libdir@"
  ;;
esac

libS=
if test @libdir@ != /usr/lib ; then
  libS=-L@libdir@
fi

while test $# -gt 0; do
  case "$1" in
  -*=*) optarg=`echo "$1" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
  *) optarg= ;;
  esac

  case $1 in
    --prefix=*)
      prefix=$optarg
      if test $exec_prefix_set = no ; then
        exec_prefix=$optarg
      fi
      ;;
    --prefix)
      echo $prefix
      ;;
    --exec-prefix=*)
      exec_prefix=$optarg
      exec_prefix_set=yes
      ;;
    --exec-prefix)
      echo $exec_prefix
      ;;
    --version)
      echo @PACKAGE_VERSION@
      ;;
    --cflags)
      if test @includedir@ != /usr/include ; then
        includes=-I@includedir@
      fi
      echo $includes @PCRE2_STATIC_CFLAG@
      ;;
    --cflags-posix)
      if test @enable_pcre2_8@ = yes ; then
        if test @includedir@ != /usr/include ; then
          includes=-I@includedir@
        fi
        echo $includes @PCRE2_STATIC_CFLAG@
      else
        echo "${usage}" 1>&2
      fi
      ;;
    --libs-posix)
      if test @enable_pcre2_8@ = yes ; then
        echo $libS$libR -lpcre2posix -lpcre2-8
      else
        echo "${usage}" 1>&2
      fi
      ;;
    --libs8)
      if test @enable_pcre2_8@ = yes ; then
        echo $libS$libR -lpcre2-8
      else
        echo "${usage}" 1>&2
      fi
      ;;
    --libs16)
      if test @enable_pcre2_16@ = yes ; then
        echo $libS$libR -lpcre2-16
      else
        echo "${usage}" 1>&2
      fi
      ;;
    --libs32)
      if test @enable_pcre2_32@ = yes ; then
        echo $libS$libR -lpcre2-32
      else
        echo "${usage}" 1>&2
      fi
      ;;
    *)
      echo "${usage}" 1>&2
      exit 1
      ;;
  esac
  shift
done
