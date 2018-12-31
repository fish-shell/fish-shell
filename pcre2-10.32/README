README file for PCRE2 (Perl-compatible regular expression library)
------------------------------------------------------------------

PCRE2 is a re-working of the original PCRE library to provide an entirely new
API. The latest release of PCRE2 is always available in three alternative
formats from:

  ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-xxx.tar.gz
  ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-xxx.tar.bz2
  ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-xxx.zip

There is a mailing list for discussion about the development of PCRE (both the
original and new APIs) at pcre-dev@exim.org. You can access the archives and
subscribe or manage your subscription here:

   https://lists.exim.org/mailman/listinfo/pcre-dev

Please read the NEWS file if you are upgrading from a previous release. The
contents of this README file are:

  The PCRE2 APIs
  Documentation for PCRE2
  Contributions by users of PCRE2
  Building PCRE2 on non-Unix-like systems
  Building PCRE2 without using autotools
  Building PCRE2 using autotools
  Retrieving configuration information
  Shared libraries
  Cross-compiling using autotools
  Making new tarballs
  Testing PCRE2
  Character tables
  File manifest


The PCRE2 APIs
--------------

PCRE2 is written in C, and it has its own API. There are three sets of
functions, one for the 8-bit library, which processes strings of bytes, one for
the 16-bit library, which processes strings of 16-bit values, and one for the
32-bit library, which processes strings of 32-bit values. There are no C++
wrappers.

The distribution does contain a set of C wrapper functions for the 8-bit
library that are based on the POSIX regular expression API (see the pcre2posix
man page). These can be found in a library called libpcre2-posix. Note that
this just provides a POSIX calling interface to PCRE2; the regular expressions
themselves still follow Perl syntax and semantics. The POSIX API is restricted,
and does not give full access to all of PCRE2's facilities.

The header file for the POSIX-style functions is called pcre2posix.h. The
official POSIX name is regex.h, but I did not want to risk possible problems
with existing files of that name by distributing it that way. To use PCRE2 with
an existing program that uses the POSIX API, pcre2posix.h will have to be
renamed or pointed at by a link.

If you are using the POSIX interface to PCRE2 and there is already a POSIX
regex library installed on your system, as well as worrying about the regex.h
header file (as mentioned above), you must also take care when linking programs
to ensure that they link with PCRE2's libpcre2-posix library. Otherwise they
may pick up the POSIX functions of the same name from the other library.

One way of avoiding this confusion is to compile PCRE2 with the addition of
-Dregcomp=PCRE2regcomp (and similarly for the other POSIX functions) to the
compiler flags (CFLAGS if you are using "configure" -- see below). This has the
effect of renaming the functions so that the names no longer clash. Of course,
you have to do the same thing for your applications, or write them using the
new names.


Documentation for PCRE2
-----------------------

If you install PCRE2 in the normal way on a Unix-like system, you will end up
with a set of man pages whose names all start with "pcre2". The one that is
just called "pcre2" lists all the others. In addition to these man pages, the
PCRE2 documentation is supplied in two other forms:

  1. There are files called doc/pcre2.txt, doc/pcre2grep.txt, and
     doc/pcre2test.txt in the source distribution. The first of these is a
     concatenation of the text forms of all the section 3 man pages except the
     listing of pcre2demo.c and those that summarize individual functions. The
     other two are the text forms of the section 1 man pages for the pcre2grep
     and pcre2test commands. These text forms are provided for ease of scanning
     with text editors or similar tools. They are installed in
     <prefix>/share/doc/pcre2, where <prefix> is the installation prefix
     (defaulting to /usr/local).

  2. A set of files containing all the documentation in HTML form, hyperlinked
     in various ways, and rooted in a file called index.html, is distributed in
     doc/html and installed in <prefix>/share/doc/pcre2/html.


Building PCRE2 on non-Unix-like systems
---------------------------------------

For a non-Unix-like system, please read the file NON-AUTOTOOLS-BUILD, though if
your system supports the use of "configure" and "make" you may be able to build
PCRE2 using autotools in the same way as for many Unix-like systems.

PCRE2 can also be configured using CMake, which can be run in various ways
(command line, GUI, etc). This creates Makefiles, solution files, etc. The file
NON-AUTOTOOLS-BUILD has information about CMake.

PCRE2 has been compiled on many different operating systems. It should be
straightforward to build PCRE2 on any system that has a Standard C compiler and
library, because it uses only Standard C functions.


Building PCRE2 without using autotools
--------------------------------------

The use of autotools (in particular, libtool) is problematic in some
environments, even some that are Unix or Unix-like. See the NON-AUTOTOOLS-BUILD
file for ways of building PCRE2 without using autotools.


Building PCRE2 using autotools
------------------------------

The following instructions assume the use of the widely used "configure; make;
make install" (autotools) process.

To build PCRE2 on system that supports autotools, first run the "configure"
command from the PCRE2 distribution directory, with your current directory set
to the directory where you want the files to be created. This command is a
standard GNU "autoconf" configuration script, for which generic instructions
are supplied in the file INSTALL.

Most commonly, people build PCRE2 within its own distribution directory, and in
this case, on many systems, just running "./configure" is sufficient. However,
the usual methods of changing standard defaults are available. For example:

CFLAGS='-O2 -Wall' ./configure --prefix=/opt/local

This command specifies that the C compiler should be run with the flags '-O2
-Wall' instead of the default, and that "make install" should install PCRE2
under /opt/local instead of the default /usr/local.

If you want to build in a different directory, just run "configure" with that
directory as current. For example, suppose you have unpacked the PCRE2 source
into /source/pcre2/pcre2-xxx, but you want to build it in
/build/pcre2/pcre2-xxx:

cd /build/pcre2/pcre2-xxx
/source/pcre2/pcre2-xxx/configure

PCRE2 is written in C and is normally compiled as a C library. However, it is
possible to build it as a C++ library, though the provided building apparatus
does not have any features to support this.

There are some optional features that can be included or omitted from the PCRE2
library. They are also documented in the pcre2build man page.

. By default, both shared and static libraries are built. You can change this
  by adding one of these options to the "configure" command:

  --disable-shared
  --disable-static

  (See also "Shared libraries on Unix-like systems" below.)

. By default, only the 8-bit library is built. If you add --enable-pcre2-16 to
  the "configure" command, the 16-bit library is also built. If you add
  --enable-pcre2-32 to the "configure" command, the 32-bit library is also
  built. If you want only the 16-bit or 32-bit library, use --disable-pcre2-8
  to disable building the 8-bit library.

. If you want to include support for just-in-time (JIT) compiling, which can
  give large performance improvements on certain platforms, add --enable-jit to
  the "configure" command. This support is available only for certain hardware
  architectures. If you try to enable it on an unsupported architecture, there
  will be a compile time error. If in doubt, use --enable-jit=auto, which
  enables JIT only if the current hardware is supported.

. If you are enabling JIT under SELinux you may also want to add
  --enable-jit-sealloc, which enables the use of an execmem allocator in JIT
  that is compatible with SELinux. This has no effect if JIT is not enabled.

. If you do not want to make use of the default support for UTF-8 Unicode
  character strings in the 8-bit library, UTF-16 Unicode character strings in
  the 16-bit library, or UTF-32 Unicode character strings in the 32-bit
  library, you can add --disable-unicode to the "configure" command. This
  reduces the size of the libraries. It is not possible to configure one
  library with Unicode support, and another without, in the same configuration.
  It is also not possible to use --enable-ebcdic (see below) with Unicode
  support, so if this option is set, you must also use --disable-unicode.

  When Unicode support is available, the use of a UTF encoding still has to be
  enabled by setting the PCRE2_UTF option at run time or starting a pattern
  with (*UTF). When PCRE2 is compiled with Unicode support, its input can only
  either be ASCII or UTF-8/16/32, even when running on EBCDIC platforms.

  As well as supporting UTF strings, Unicode support includes support for the
  \P, \p, and \X sequences that recognize Unicode character properties.
  However, only the basic two-letter properties such as Lu are supported.
  Escape sequences such as \d and \w in patterns do not by default make use of
  Unicode properties, but can be made to do so by setting the PCRE2_UCP option
  or starting a pattern with (*UCP).

. You can build PCRE2 to recognize either CR or LF or the sequence CRLF, or any
  of the preceding, or any of the Unicode newline sequences, or the NUL (zero)
  character as indicating the end of a line. Whatever you specify at build time
  is the default; the caller of PCRE2 can change the selection at run time. The
  default newline indicator is a single LF character (the Unix standard). You
  can specify the default newline indicator by adding --enable-newline-is-cr,
  --enable-newline-is-lf, --enable-newline-is-crlf,
  --enable-newline-is-anycrlf, --enable-newline-is-any, or
  --enable-newline-is-nul to the "configure" command, respectively.

. By default, the sequence \R in a pattern matches any Unicode line ending
  sequence. This is independent of the option specifying what PCRE2 considers
  to be the end of a line (see above). However, the caller of PCRE2 can
  restrict \R to match only CR, LF, or CRLF. You can make this the default by
  adding --enable-bsr-anycrlf to the "configure" command (bsr = "backslash R").

. In a pattern, the escape sequence \C matches a single code unit, even in a
  UTF mode. This can be dangerous because it breaks up multi-code-unit
  characters. You can build PCRE2 with the use of \C permanently locked out by
  adding --enable-never-backslash-C (note the upper case C) to the "configure"
  command. When \C is allowed by the library, individual applications can lock
  it out by calling pcre2_compile() with the PCRE2_NEVER_BACKSLASH_C option.

. PCRE2 has a counter that limits the depth of nesting of parentheses in a
  pattern. This limits the amount of system stack that a pattern uses when it
  is compiled. The default is 250, but you can change it by setting, for
  example,

  --with-parens-nest-limit=500

. PCRE2 has a counter that can be set to limit the amount of computing resource
  it uses when matching a pattern. If the limit is exceeded during a match, the
  match fails. The default is ten million. You can change the default by
  setting, for example,

  --with-match-limit=500000

  on the "configure" command. This is just the default; individual calls to
  pcre2_match() or pcre2_dfa_match() can supply their own value. There is more
  discussion in the pcre2api man page (search for pcre2_set_match_limit).

. There is a separate counter that limits the depth of nested backtracking
  (pcre2_match()) or nested function calls (pcre2_dfa_match()) during a
  matching process, which indirectly limits the amount of heap memory that is
  used, and in the case of pcre2_dfa_match() the amount of stack as well. This
  counter also has a default of ten million, which is essentially "unlimited".
  You can change the default by setting, for example,

  --with-match-limit-depth=5000

  There is more discussion in the pcre2api man page (search for
  pcre2_set_depth_limit).

. You can also set an explicit limit on the amount of heap memory used by
  the pcre2_match() and pcre2_dfa_match() interpreters:

  --with-heap-limit=500

  The units are kibibytes (units of 1024 bytes). This limit does not apply when
  the JIT optimization (which has its own memory control features) is used.
  There is more discussion on the pcre2api man page (search for
  pcre2_set_heap_limit).

. In the 8-bit library, the default maximum compiled pattern size is around
  64 kibibytes. You can increase this by adding --with-link-size=3 to the
  "configure" command. PCRE2 then uses three bytes instead of two for offsets
  to different parts of the compiled pattern. In the 16-bit library,
  --with-link-size=3 is the same as --with-link-size=4, which (in both
  libraries) uses four-byte offsets. Increasing the internal link size reduces
  performance in the 8-bit and 16-bit libraries. In the 32-bit library, the
  link size setting is ignored, as 4-byte offsets are always used.

. For speed, PCRE2 uses four tables for manipulating and identifying characters
  whose code point values are less than 256. By default, it uses a set of
  tables for ASCII encoding that is part of the distribution. If you specify

  --enable-rebuild-chartables

  a program called dftables is compiled and run in the default C locale when
  you obey "make". It builds a source file called pcre2_chartables.c. If you do
  not specify this option, pcre2_chartables.c is created as a copy of
  pcre2_chartables.c.dist. See "Character tables" below for further
  information.

. It is possible to compile PCRE2 for use on systems that use EBCDIC as their
  character code (as opposed to ASCII/Unicode) by specifying

  --enable-ebcdic --disable-unicode

  This automatically implies --enable-rebuild-chartables (see above). However,
  when PCRE2 is built this way, it always operates in EBCDIC. It cannot support
  both EBCDIC and UTF-8/16/32. There is a second option, --enable-ebcdic-nl25,
  which specifies that the code value for the EBCDIC NL character is 0x25
  instead of the default 0x15.

. If you specify --enable-debug, additional debugging code is included in the
  build. This option is intended for use by the PCRE2 maintainers.

. In environments where valgrind is installed, if you specify

  --enable-valgrind

  PCRE2 will use valgrind annotations to mark certain memory regions as
  unaddressable. This allows it to detect invalid memory accesses, and is
  mostly useful for debugging PCRE2 itself.

. In environments where the gcc compiler is used and lcov version 1.6 or above
  is installed, if you specify

  --enable-coverage

  the build process implements a code coverage report for the test suite. The
  report is generated by running "make coverage". If ccache is installed on
  your system, it must be disabled when building PCRE2 for coverage reporting.
  You can do this by setting the environment variable CCACHE_DISABLE=1 before
  running "make" to build PCRE2. There is more information about coverage
  reporting in the "pcre2build" documentation.

. When JIT support is enabled, pcre2grep automatically makes use of it, unless
  you add --disable-pcre2grep-jit to the "configure" command.

. There is support for calling external programs during matching in the
  pcre2grep command, using PCRE2's callout facility with string arguments. This
  support can be disabled by adding --disable-pcre2grep-callout to the
  "configure" command.

. The pcre2grep program currently supports only 8-bit data files, and so
  requires the 8-bit PCRE2 library. It is possible to compile pcre2grep to use
  libz and/or libbz2, in order to read .gz and .bz2 files (respectively), by
  specifying one or both of

  --enable-pcre2grep-libz
  --enable-pcre2grep-libbz2

  Of course, the relevant libraries must be installed on your system.

. The default starting size (in bytes) of the internal buffer used by pcre2grep
  can be set by, for example:

  --with-pcre2grep-bufsize=51200

  The value must be a plain integer. The default is 20480. The amount of memory
  used by pcre2grep is actually three times this number, to allow for "before"
  and "after" lines. If very long lines are encountered, the buffer is
  automatically enlarged, up to a fixed maximum size.

. The default maximum size of pcre2grep's internal buffer can be set by, for
  example:

  --with-pcre2grep-max-bufsize=2097152

  The default is either 1048576 or the value of --with-pcre2grep-bufsize,
  whichever is the larger.

. It is possible to compile pcre2test so that it links with the libreadline
  or libedit libraries, by specifying, respectively,

  --enable-pcre2test-libreadline or --enable-pcre2test-libedit

  If this is done, when pcre2test's input is from a terminal, it reads it using
  the readline() function. This provides line-editing and history facilities.
  Note that libreadline is GPL-licenced, so if you distribute a binary of
  pcre2test linked in this way, there may be licensing issues. These can be
  avoided by linking with libedit (which has a BSD licence) instead.

  Enabling libreadline causes the -lreadline option to be added to the
  pcre2test build. In many operating environments with a sytem-installed
  readline library this is sufficient. However, in some environments (e.g. if
  an unmodified distribution version of readline is in use), it may be
  necessary to specify something like LIBS="-lncurses" as well. This is
  because, to quote the readline INSTALL, "Readline uses the termcap functions,
  but does not link with the termcap or curses library itself, allowing
  applications which link with readline the to choose an appropriate library."
  If you get error messages about missing functions tgetstr, tgetent, tputs,
  tgetflag, or tgoto, this is the problem, and linking with the ncurses library
  should fix it.

. There is a special option called --enable-fuzz-support for use by people who
  want to run fuzzing tests on PCRE2. At present this applies only to the 8-bit
  library. If set, it causes an extra library called libpcre2-fuzzsupport.a to
  be built, but not installed. This contains a single function called
  LLVMFuzzerTestOneInput() whose arguments are a pointer to a string and the
  length of the string. When called, this function tries to compile the string
  as a pattern, and if that succeeds, to match it. This is done both with no
  options and with some random options bits that are generated from the string.
  Setting --enable-fuzz-support also causes a binary called pcre2fuzzcheck to
  be created. This is normally run under valgrind or used when PCRE2 is
  compiled with address sanitizing enabled. It calls the fuzzing function and
  outputs information about it is doing. The input strings are specified by
  arguments: if an argument starts with "=" the rest of it is a literal input
  string. Otherwise, it is assumed to be a file name, and the contents of the
  file are the test string.

. Releases before 10.30 could be compiled with --disable-stack-for-recursion,
  which caused pcre2_match() to use individual blocks on the heap for
  backtracking instead of recursive function calls (which use the stack). This
  is now obsolete since pcre2_match() was refactored always to use the heap (in
  a much more efficient way than before). This option is retained for backwards
  compatibility, but has no effect other than to output a warning.

The "configure" script builds the following files for the basic C library:

. Makefile             the makefile that builds the library
. src/config.h         build-time configuration options for the library
. src/pcre2.h          the public PCRE2 header file
. pcre2-config          script that shows the building settings such as CFLAGS
                         that were set for "configure"
. libpcre2-8.pc        )
. libpcre2-16.pc       ) data for the pkg-config command
. libpcre2-32.pc       )
. libpcre2-posix.pc    )
. libtool              script that builds shared and/or static libraries

Versions of config.h and pcre2.h are distributed in the src directory of PCRE2
tarballs under the names config.h.generic and pcre2.h.generic. These are
provided for those who have to build PCRE2 without using "configure" or CMake.
If you use "configure" or CMake, the .generic versions are not used.

The "configure" script also creates config.status, which is an executable
script that can be run to recreate the configuration, and config.log, which
contains compiler output from tests that "configure" runs.

Once "configure" has run, you can run "make". This builds whichever of the
libraries libpcre2-8, libpcre2-16 and libpcre2-32 are configured, and a test
program called pcre2test. If you enabled JIT support with --enable-jit, another
test program called pcre2_jit_test is built as well. If the 8-bit library is
built, libpcre2-posix and the pcre2grep command are also built. Running
"make" with the -j option may speed up compilation on multiprocessor systems.

The command "make check" runs all the appropriate tests. Details of the PCRE2
tests are given below in a separate section of this document. The -j option of
"make" can also be used when running the tests.

You can use "make install" to install PCRE2 into live directories on your
system. The following are installed (file names are all relative to the
<prefix> that is set when "configure" is run):

  Commands (bin):
    pcre2test
    pcre2grep (if 8-bit support is enabled)
    pcre2-config

  Libraries (lib):
    libpcre2-8      (if 8-bit support is enabled)
    libpcre2-16     (if 16-bit support is enabled)
    libpcre2-32     (if 32-bit support is enabled)
    libpcre2-posix  (if 8-bit support is enabled)

  Configuration information (lib/pkgconfig):
    libpcre2-8.pc
    libpcre2-16.pc
    libpcre2-32.pc
    libpcre2-posix.pc

  Header files (include):
    pcre2.h
    pcre2posix.h

  Man pages (share/man/man{1,3}):
    pcre2grep.1
    pcre2test.1
    pcre2-config.1
    pcre2.3
    pcre2*.3 (lots more pages, all starting "pcre2")

  HTML documentation (share/doc/pcre2/html):
    index.html
    *.html (lots more pages, hyperlinked from index.html)

  Text file documentation (share/doc/pcre2):
    AUTHORS
    COPYING
    ChangeLog
    LICENCE
    NEWS
    README
    pcre2.txt         (a concatenation of the man(3) pages)
    pcre2test.txt     the pcre2test man page
    pcre2grep.txt     the pcre2grep man page
    pcre2-config.txt  the pcre2-config man page

If you want to remove PCRE2 from your system, you can run "make uninstall".
This removes all the files that "make install" installed. However, it does not
remove any directories, because these are often shared with other programs.


Retrieving configuration information
------------------------------------

Running "make install" installs the command pcre2-config, which can be used to
recall information about the PCRE2 configuration and installation. For example:

  pcre2-config --version

prints the version number, and

  pcre2-config --libs8

outputs information about where the 8-bit library is installed. This command
can be included in makefiles for programs that use PCRE2, saving the programmer
from having to remember too many details. Run pcre2-config with no arguments to
obtain a list of possible arguments.

The pkg-config command is another system for saving and retrieving information
about installed libraries. Instead of separate commands for each library, a
single command is used. For example:

  pkg-config --libs libpcre2-16

The data is held in *.pc files that are installed in a directory called
<prefix>/lib/pkgconfig.


Shared libraries
----------------

The default distribution builds PCRE2 as shared libraries and static libraries,
as long as the operating system supports shared libraries. Shared library
support relies on the "libtool" script which is built as part of the
"configure" process.

The libtool script is used to compile and link both shared and static
libraries. They are placed in a subdirectory called .libs when they are newly
built. The programs pcre2test and pcre2grep are built to use these uninstalled
libraries (by means of wrapper scripts in the case of shared libraries). When
you use "make install" to install shared libraries, pcre2grep and pcre2test are
automatically re-built to use the newly installed shared libraries before being
installed themselves. However, the versions left in the build directory still
use the uninstalled libraries.

To build PCRE2 using static libraries only you must use --disable-shared when
configuring it. For example:

./configure --prefix=/usr/gnu --disable-shared

Then run "make" in the usual way. Similarly, you can use --disable-static to
build only shared libraries.


Cross-compiling using autotools
-------------------------------

You can specify CC and CFLAGS in the normal way to the "configure" command, in
order to cross-compile PCRE2 for some other host. However, you should NOT
specify --enable-rebuild-chartables, because if you do, the dftables.c source
file is compiled and run on the local host, in order to generate the inbuilt
character tables (the pcre2_chartables.c file). This will probably not work,
because dftables.c needs to be compiled with the local compiler, not the cross
compiler.

When --enable-rebuild-chartables is not specified, pcre2_chartables.c is
created by making a copy of pcre2_chartables.c.dist, which is a default set of
tables that assumes ASCII code. Cross-compiling with the default tables should
not be a problem.

If you need to modify the character tables when cross-compiling, you should
move pcre2_chartables.c.dist out of the way, then compile dftables.c by hand
and run it on the local host to make a new version of pcre2_chartables.c.dist.
Then when you cross-compile PCRE2 this new version of the tables will be used.


Making new tarballs
-------------------

The command "make dist" creates three PCRE2 tarballs, in tar.gz, tar.bz2, and
zip formats. The command "make distcheck" does the same, but then does a trial
build of the new distribution to ensure that it works.

If you have modified any of the man page sources in the doc directory, you
should first run the PrepareRelease script before making a distribution. This
script creates the .txt and HTML forms of the documentation from the man pages.


Testing PCRE2
-------------

To test the basic PCRE2 library on a Unix-like system, run the RunTest script.
There is another script called RunGrepTest that tests the pcre2grep command.
When JIT support is enabled, a third test program called pcre2_jit_test is
built. Both the scripts and all the program tests are run if you obey "make
check". For other environments, see the instructions in NON-AUTOTOOLS-BUILD.

The RunTest script runs the pcre2test test program (which is documented in its
own man page) on each of the relevant testinput files in the testdata
directory, and compares the output with the contents of the corresponding
testoutput files. RunTest uses a file called testtry to hold the main output
from pcre2test. Other files whose names begin with "test" are used as working
files in some tests.

Some tests are relevant only when certain build-time options were selected. For
example, the tests for UTF-8/16/32 features are run only when Unicode support
is available. RunTest outputs a comment when it skips a test.

Many (but not all) of the tests that are not skipped are run twice if JIT
support is available. On the second run, JIT compilation is forced. This
testing can be suppressed by putting "nojit" on the RunTest command line.

The entire set of tests is run once for each of the 8-bit, 16-bit and 32-bit
libraries that are enabled. If you want to run just one set of tests, call
RunTest with either the -8, -16 or -32 option.

If valgrind is installed, you can run the tests under it by putting "valgrind"
on the RunTest command line. To run pcre2test on just one or more specific test
files, give their numbers as arguments to RunTest, for example:

  RunTest 2 7 11

You can also specify ranges of tests such as 3-6 or 3- (meaning 3 to the
end), or a number preceded by ~ to exclude a test. For example:

  Runtest 3-15 ~10

This runs tests 3 to 15, excluding test 10, and just ~13 runs all the tests
except test 13. Whatever order the arguments are in, the tests are always run
in numerical order.

You can also call RunTest with the single argument "list" to cause it to output
a list of tests.

The test sequence starts with "test 0", which is a special test that has no
input file, and whose output is not checked. This is because it will be
different on different hardware and with different configurations. The test
exists in order to exercise some of pcre2test's code that would not otherwise
be run.

Tests 1 and 2 can always be run, as they expect only plain text strings (not
UTF) and make no use of Unicode properties. The first test file can be fed
directly into the perltest.sh script to check that Perl gives the same results.
The only difference you should see is in the first few lines, where the Perl
version is given instead of the PCRE2 version. The second set of tests check
auxiliary functions, error detection, and run-time flags that are specific to
PCRE2. It also uses the debugging flags to check some of the internals of
pcre2_compile().

If you build PCRE2 with a locale setting that is not the standard C locale, the
character tables may be different (see next paragraph). In some cases, this may
cause failures in the second set of tests. For example, in a locale where the
isprint() function yields TRUE for characters in the range 128-255, the use of
[:isascii:] inside a character class defines a different set of characters, and
this shows up in this test as a difference in the compiled code, which is being
listed for checking. For example, where the comparison test output contains
[\x00-\x7f] the test might contain [\x00-\xff], and similarly in some other
cases. This is not a bug in PCRE2.

Test 3 checks pcre2_maketables(), the facility for building a set of character
tables for a specific locale and using them instead of the default tables. The
script uses the "locale" command to check for the availability of the "fr_FR",
"french", or "fr" locale, and uses the first one that it finds. If the "locale"
command fails, or if its output doesn't include "fr_FR", "french", or "fr" in
the list of available locales, the third test cannot be run, and a comment is
output to say why. If running this test produces an error like this:

  ** Failed to set locale "fr_FR"

it means that the given locale is not available on your system, despite being
listed by "locale". This does not mean that PCRE2 is broken. There are three
alternative output files for the third test, because three different versions
of the French locale have been encountered. The test passes if its output
matches any one of them.

Tests 4 and 5 check UTF and Unicode property support, test 4 being compatible
with the perltest.sh script, and test 5 checking PCRE2-specific things.

Tests 6 and 7 check the pcre2_dfa_match() alternative matching function, in
non-UTF mode and UTF-mode with Unicode property support, respectively.

Test 8 checks some internal offsets and code size features, but it is run only
when Unicode support is enabled. The output is different in 8-bit, 16-bit, and
32-bit modes and for different link sizes, so there are different output files
for each mode and link size.

Tests 9 and 10 are run only in 8-bit mode, and tests 11 and 12 are run only in
16-bit and 32-bit modes. These are tests that generate different output in
8-bit mode. Each pair are for general cases and Unicode support, respectively.

Test 13 checks the handling of non-UTF characters greater than 255 by
pcre2_dfa_match() in 16-bit and 32-bit modes.

Test 14 contains some special UTF and UCP tests that give different output for
different code unit widths.

Test 15 contains a number of tests that must not be run with JIT. They check,
among other non-JIT things, the match-limiting features of the intepretive
matcher.

Test 16 is run only when JIT support is not available. It checks that an
attempt to use JIT has the expected behaviour.

Test 17 is run only when JIT support is available. It checks JIT complete and
partial modes, match-limiting under JIT, and other JIT-specific features.

Tests 18 and 19 are run only in 8-bit mode. They check the POSIX interface to
the 8-bit library, without and with Unicode support, respectively.

Test 20 checks the serialization functions by writing a set of compiled
patterns to a file, and then reloading and checking them.

Tests 21 and 22 test \C support when the use of \C is not locked out, without
and with UTF support, respectively. Test 23 tests \C when it is locked out.

Tests 24 and 25 test the experimental pattern conversion functions, without and
with UTF support, respectively.


Character tables
----------------

For speed, PCRE2 uses four tables for manipulating and identifying characters
whose code point values are less than 256. By default, a set of tables that is
built into the library is used. The pcre2_maketables() function can be called
by an application to create a new set of tables in the current locale. This are
passed to PCRE2 by calling pcre2_set_character_tables() to put a pointer into a
compile context.

The source file called pcre2_chartables.c contains the default set of tables.
By default, this is created as a copy of pcre2_chartables.c.dist, which
contains tables for ASCII coding. However, if --enable-rebuild-chartables is
specified for ./configure, a different version of pcre2_chartables.c is built
by the program dftables (compiled from dftables.c), which uses the ANSI C
character handling functions such as isalnum(), isalpha(), isupper(),
islower(), etc. to build the table sources. This means that the default C
locale that is set for your system will control the contents of these default
tables. You can change the default tables by editing pcre2_chartables.c and
then re-building PCRE2. If you do this, you should take care to ensure that the
file does not get automatically re-generated. The best way to do this is to
move pcre2_chartables.c.dist out of the way and replace it with your customized
tables.

When the dftables program is run as a result of --enable-rebuild-chartables,
it uses the default C locale that is set on your system. It does not pay
attention to the LC_xxx environment variables. In other words, it uses the
system's default locale rather than whatever the compiling user happens to have
set. If you really do want to build a source set of character tables in a
locale that is specified by the LC_xxx variables, you can run the dftables
program by hand with the -L option. For example:

  ./dftables -L pcre2_chartables.c.special

The first two 256-byte tables provide lower casing and case flipping functions,
respectively. The next table consists of three 32-byte bit maps which identify
digits, "word" characters, and white space, respectively. These are used when
building 32-byte bit maps that represent character classes for code points less
than 256. The final 256-byte table has bits indicating various character types,
as follows:

    1   white space character
    2   letter
    4   decimal digit
    8   hexadecimal digit
   16   alphanumeric or '_'
  128   regular expression metacharacter or binary zero

You should not alter the set of characters that contain the 128 bit, as that
will cause PCRE2 to malfunction.


File manifest
-------------

The distribution should contain the files listed below.

(A) Source files for the PCRE2 library functions and their headers are found in
    the src directory:

  src/dftables.c           auxiliary program for building pcre2_chartables.c
                           when --enable-rebuild-chartables is specified

  src/pcre2_chartables.c.dist  a default set of character tables that assume
                           ASCII coding; unless --enable-rebuild-chartables is
                           specified, used by copying to pcre2_chartables.c

  src/pcre2posix.c         )
  src/pcre2_auto_possess.c )
  src/pcre2_compile.c      )
  src/pcre2_config.c       )
  src/pcre2_context.c      )
  src/pcre2_convert.c      )
  src/pcre2_dfa_match.c    )
  src/pcre2_error.c        )
  src/pcre2_extuni.c       )
  src/pcre2_find_bracket.c )
  src/pcre2_jit_compile.c  )
  src/pcre2_jit_match.c    ) sources for the functions in the library,
  src/pcre2_jit_misc.c     )   and some internal functions that they use
  src/pcre2_maketables.c   )
  src/pcre2_match.c        )
  src/pcre2_match_data.c   )
  src/pcre2_newline.c      )
  src/pcre2_ord2utf.c      )
  src/pcre2_pattern_info.c )
  src/pcre2_serialize.c    )
  src/pcre2_string_utils.c )
  src/pcre2_study.c        )
  src/pcre2_substitute.c   )
  src/pcre2_substring.c    )
  src/pcre2_tables.c       )
  src/pcre2_ucd.c          )
  src/pcre2_valid_utf.c    )
  src/pcre2_xclass.c       )

  src/pcre2_printint.c     debugging function that is used by pcre2test,
  src/pcre2_fuzzsupport.c  function for (optional) fuzzing support

  src/config.h.in          template for config.h, when built by "configure"
  src/pcre2.h.in           template for pcre2.h when built by "configure"
  src/pcre2posix.h         header for the external POSIX wrapper API
  src/pcre2_internal.h     header for internal use
  src/pcre2_intmodedep.h   a mode-specific internal header
  src/pcre2_ucp.h          header for Unicode property handling

  sljit/*                  source files for the JIT compiler

(B) Source files for programs that use PCRE2:

  src/pcre2demo.c          simple demonstration of coding calls to PCRE2
  src/pcre2grep.c          source of a grep utility that uses PCRE2
  src/pcre2test.c          comprehensive test program
  src/pcre2_jit_test.c     JIT test program

(C) Auxiliary files:

  132html                  script to turn "man" pages into HTML
  AUTHORS                  information about the author of PCRE2
  ChangeLog                log of changes to the code
  CleanTxt                 script to clean nroff output for txt man pages
  Detrail                  script to remove trailing spaces
  HACKING                  some notes about the internals of PCRE2
  INSTALL                  generic installation instructions
  LICENCE                  conditions for the use of PCRE2
  COPYING                  the same, using GNU's standard name
  Makefile.in              ) template for Unix Makefile, which is built by
                           )   "configure"
  Makefile.am              ) the automake input that was used to create
                           )   Makefile.in
  NEWS                     important changes in this release
  NON-AUTOTOOLS-BUILD      notes on building PCRE2 without using autotools
  PrepareRelease           script to make preparations for "make dist"
  README                   this file
  RunTest                  a Unix shell script for running tests
  RunGrepTest              a Unix shell script for pcre2grep tests
  aclocal.m4               m4 macros (generated by "aclocal")
  config.guess             ) files used by libtool,
  config.sub               )   used only when building a shared library
  configure                a configuring shell script (built by autoconf)
  configure.ac             ) the autoconf input that was used to build
                           )   "configure" and config.h
  depcomp                  ) script to find program dependencies, generated by
                           )   automake
  doc/*.3                  man page sources for PCRE2
  doc/*.1                  man page sources for pcre2grep and pcre2test
  doc/index.html.src       the base HTML page
  doc/html/*               HTML documentation
  doc/pcre2.txt            plain text version of the man pages
  doc/pcre2test.txt        plain text documentation of test program
  install-sh               a shell script for installing files
  libpcre2-8.pc.in         template for libpcre2-8.pc for pkg-config
  libpcre2-16.pc.in        template for libpcre2-16.pc for pkg-config
  libpcre2-32.pc.in        template for libpcre2-32.pc for pkg-config
  libpcre2-posix.pc.in     template for libpcre2-posix.pc for pkg-config
  ltmain.sh                file used to build a libtool script
  missing                  ) common stub for a few missing GNU programs while
                           )   installing, generated by automake
  mkinstalldirs            script for making install directories
  perltest.sh              Script for running a Perl test program
  pcre2-config.in          source of script which retains PCRE2 information
  testdata/testinput*      test data for main library tests
  testdata/testoutput*     expected test results
  testdata/grep*           input and output for pcre2grep tests
  testdata/*               other supporting test files

(D) Auxiliary files for cmake support

  cmake/COPYING-CMAKE-SCRIPTS
  cmake/FindPackageHandleStandardArgs.cmake
  cmake/FindEditline.cmake
  cmake/FindReadline.cmake
  CMakeLists.txt
  config-cmake.h.in

(E) Auxiliary files for building PCRE2 "by hand"

  src/pcre2.h.generic     ) a version of the public PCRE2 header file
                          )   for use in non-"configure" environments
  src/config.h.generic    ) a version of config.h for use in non-"configure"
                          )   environments

Philip Hazel
Email local part: ph10
Email domain: cam.ac.uk
Last updated: 17 June 2018
