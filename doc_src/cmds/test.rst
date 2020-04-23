.. _cmd-test:

test - perform tests on files and text
======================================

Synopsis
--------

::

    test [EXPRESSION]
    [ [EXPRESSION] ]


Description
-----------

Tests the expression given and sets the exit status to 0 if true, and 1 if false. An expression is made up of one or more operators and their arguments.

The first form (``test``) is preferred. For compatibility with other shells, the second form is available: a matching pair of square brackets (``[ [EXPRESSION ] ]``).

This test is mostly POSIX-compatible.

When using a variable as an argument for a test operator you should almost always enclose it in double-quotes. There are only two situations it is safe to omit the quote marks. The first is when the argument is a literal string with no whitespace or other characters special to the shell (e.g., semicolon). For example, ``test -b /my/file``. The second is using a variable that expands to exactly one element including if that element is the empty string (e.g., ``set x ''``). If the variable is not set, set but with no value, or set to more than one value you must enclose it in double-quotes. For example, ``test "$x" = "$y"``. Since it is always safe to enclose variables in double-quotes when used as ``test`` arguments that is the recommended practice.

Operators for files and directories
-----------------------------------

- ``-b FILE`` returns true if ``FILE`` is a block device.

- ``-c FILE`` returns true if ``FILE`` is a character device.

- ``-d FILE`` returns true if ``FILE`` is a directory.

- ``-e FILE`` returns true if ``FILE`` exists.

- ``-f FILE`` returns true if ``FILE`` is a regular file.

- ``-g FILE`` returns true if ``FILE`` has the set-group-ID bit set.

- ``-G FILE`` returns true if ``FILE`` exists and has the same group ID as the current user.

- ``-k FILE`` returns true if ``FILE`` has the sticky bit set. If the OS does not support the concept it returns false. See https://en.wikipedia.org/wiki/Sticky_bit.

- ``-L FILE`` returns true if ``FILE`` is a symbolic link.

- ``-O FILE`` returns true if ``FILE`` exists and is owned by the current user.

- ``-p FILE`` returns true if ``FILE`` is a named pipe.

- ``-r FILE`` returns true if ``FILE`` is marked as readable.

- ``-s FILE`` returns true if the size of ``FILE`` is greater than zero.

- ``-S FILE`` returns true if ``FILE`` is a socket.

- ``-t FD`` returns true if the file descriptor ``FD`` is a terminal (TTY).

- ``-u FILE`` returns true if ``FILE`` has the set-user-ID bit set.

- ``-w FILE`` returns true if ``FILE`` is marked as writable; note that this does not check if the filesystem is read-only.

- ``-x FILE`` returns true if ``FILE`` is marked as executable.

Operators for text strings
--------------------------

- ``STRING1 = STRING2`` returns true if the strings ``STRING1`` and ``STRING2`` are identical.

- ``STRING1 != STRING2`` returns true if the strings ``STRING1`` and ``STRING2`` are not identical.

- ``-n STRING`` returns true if the length of ``STRING`` is non-zero.

- ``-z STRING`` returns true if the length of ``STRING`` is zero.

Operators to compare and examine numbers
----------------------------------------

- ``NUM1 -eq NUM2`` returns true if ``NUM1`` and ``NUM2`` are numerically equal.

- ``NUM1 -ne NUM2`` returns true if ``NUM1`` and ``NUM2`` are not numerically equal.

- ``NUM1 -gt NUM2`` returns true if ``NUM1`` is greater than ``NUM2``.

- ``NUM1 -ge NUM2`` returns true if ``NUM1`` is greater than or equal to ``NUM2``.

- ``NUM1 -lt NUM2`` returns true if ``NUM1`` is less than ``NUM2``.

- ``NUM1 -le NUM2`` returns true if ``NUM1`` is less than or equal to ``NUM2``.

Both integers and floating point numbers are supported.

Operators to combine expressions
--------------------------------

- ``COND1 -a COND2`` returns true if both ``COND1`` and ``COND2`` are true.

- ``COND1 -o COND2`` returns true if either ``COND1`` or ``COND2`` are true.

Expressions can be inverted using the ``!`` operator:

- ``! EXPRESSION`` returns true if ``EXPRESSION`` is false, and false if ``EXPRESSION`` is true.

Expressions can be grouped using parentheses.

- ``( EXPRESSION )`` returns the value of ``EXPRESSION``.

 Note that parentheses will usually require escaping with ``\(`` to avoid being interpreted as a command substitution.


Examples
--------

If the ``/tmp`` directory exists, copy the ``/etc/motd`` file to it:



::

    if test -d /tmp
        cp /etc/motd /tmp/motd
    end


If the variable ``MANPATH`` is defined and not empty, print the contents. (If ``MANPATH`` is not defined, then it will expand to zero arguments, unless quoted.)



::

    if test -n "$MANPATH"
        echo $MANPATH
    end


Parentheses and the ``-o`` and ``-a`` operators can be combined to produce more complicated expressions. In this example, success is printed if there is a ``/foo`` or ``/bar`` file as well as a ``/baz`` or ``/bat`` file.



::

    if test \( -f /foo -o -f /bar \) -a \( -f /baz -o -f /bat \)
        echo Success.
    end.


Numerical comparisons will simply fail if one of the operands is not a number:



::

    if test 42 -eq "The answer to life, the universe and everything"
        echo So long and thanks for all the fish # will not be executed
    end


A common comparison is with $status:



::

    if test $status -eq 0
        echo "Previous command succeeded"
    end


The previous test can likewise be inverted:



::

    if test ! $status -eq 0
        echo "Previous command failed"
    end


which is logically equivalent to the following:



::

    if test $status -ne 0
        echo "Previous command failed"
    end


Standards
---------

``test`` implements a subset of the `IEEE Std 1003.1-2008 (POSIX.1) standard <https://www.unix.com/man-page/posix/1p/test/>`__. The following exceptions apply:

- The ``<`` and ``>`` operators for comparing strings are not implemented.

- Because this test is a shell builtin and not a standalone utility, using the -c flag on a special file descriptors like standard input and output may not return the same result when invoked from within a pipe as one would expect when invoking the ``test`` utility in another shell.

 In cases such as this, one can use ``command`` ``test`` to explicitly use the system's standalone ``test`` rather than this ``builtin`` ``test``.
