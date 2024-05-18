.. _cmd-test:

test - perform tests on files and text
======================================

Synopsis
--------

.. synopsis::

    test [EXPRESSION]
    [ [EXPRESSION] ]

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``test``.
          To see the documentation on any non-fish versions, use ``command man test``.

``test`` checks the given conditions and sets the exit status to 0 if they are true, 1 if they are false.

The first form (``test``) is preferred. For compatibility with other shells, the second form is available: a matching pair of square brackets (``[ [EXPRESSION] ]``).

When using a variable or command substitution as an argument with ``test`` you should almost always enclose it in double-quotes, as variables expanding to zero or more than one argument will most likely interact badly with ``test``.

.. warning::

          For historical reasons, ``test`` supports the one-argument form (``test foo``), and this will also be triggered by e.g. ``test -n $foo`` if $foo is unset. We recommend you don't use the one-argument form and quote all variables or command substitutions used with ``test``.

          This confusing misfeature will be removed in future. ``test -n`` without any additional argument will be false, ``test -z`` will be true and any other invocation with exactly one or zero arguments, including ``test -d`` and ``test "foo"`` will be an error.

          The same goes for ``[``, e.g. ``[ "foo" ]`` and ``[ -d ]`` will be errors.

          This can be turned on already via the ``test-require-arg`` :ref:`feature flag <featureflags>`, and will eventually become the default and then only option.

Operators for files and directories
-----------------------------------

**-b** *FILE*
     Returns true if *FILE* is a block device.

**-c** *FILE*
     Returns true if *FILE* is a character device.

**-d** *FILE*
     Returns true if *FILE* is a directory.

**-e** *FILE*
     Returns true if *FILE* exists.

**-f** *FILE*
     Returns true if *FILE* is a regular file.

**-g** *FILE*
     Returns true if *FILE* has the set-group-ID bit set.

**-G** *FILE*
     Returns true if *FILE* exists and has the same group ID as the current user.

**-k** *FILE*
     Returns true if *FILE* has the sticky bit set. If the OS does not support the concept it returns false. See https://en.wikipedia.org/wiki/Sticky_bit.

**-L** *FILE*
     Returns true if *FILE* is a symbolic link.

**-O** *FILE*
     Returns true if *FILE* exists and is owned by the current user.

**-p** *FILE*
     Returns true if *FILE* is a named pipe.

**-r** *FILE*
     Returns true if *FILE* is marked as readable.

**-s** *FILE*
     Returns true if the size of *FILE* is greater than zero.

**-S** *FILE*
     Returns true if *FILE* is a socket.

**-t** *FD*
     Returns true if the file descriptor *FD* is a terminal (TTY).

**-u** *FILE*
     Returns true if *FILE* has the set-user-ID bit set.

**-w** *FILE*
     Returns true if *FILE* is marked as writable; note that this does not check if the filesystem is read-only.

**-x** *FILE*
     Returns true if *FILE* is marked as executable.

Operators to compare files and directories
------------------------------------------

*FILE1* **-nt** *FILE2*
     Returns true if *FILE1* is newer than *FILE2*, or *FILE1* exists and *FILE2* does not.

*FILE1* **-ot** *FILE2*
     Returns true if *FILE1* is older than *FILE2*, or *FILE2* exists and *FILE1* does not.

*FILE1* **-ef** *FILE1*
     Returns true if *FILE1* and *FILE2* refer to the same file.

Operators for text strings
--------------------------

*STRING1* **=** *STRING2*
     Returns true if the strings *STRING1* and *STRING2* are identical.

*STRING1* **!=** *STRING2*
     Returns true if the strings *STRING1* and *STRING2* are not identical.

**-n** *STRING*
     Returns true if the length of *STRING* is non-zero.

**-z** *STRING*
     Returns true if the length of *STRING* is zero.

Operators to compare and examine numbers
----------------------------------------

*NUM1* **-eq** *NUM2*
     Returns true if *NUM1* and *NUM2* are numerically equal.

*NUM1* **-ne** *NUM2*
     Returns true if *NUM1* and *NUM2* are not numerically equal.

*NUM1* **-gt** *NUM2*
     Returns true if *NUM1* is greater than *NUM2*.

*NUM1* **-ge** *NUM2*
     Returns true if *NUM1* is greater than or equal to *NUM2*.

*NUM1* **-lt** *NUM2*
     Returns true if *NUM1* is less than *NUM2*.

*NUM1* **-le** *NUM2*
     Returns true if *NUM1* is less than or equal to *NUM2*.

Both integers and floating point numbers are supported.

Operators to combine expressions
--------------------------------

*COND1* **-a** *COND2*
     Returns true if both *COND1* and *COND2* are true.

*COND1* **-o** *COND2*
     Returns true if either *COND1* or *COND2* are true.

Expressions can be inverted using the **!** operator:

**!** *EXPRESSION*
     Returns true if *EXPRESSION* is false, and false if *EXPRESSION* is true.

Expressions can be grouped using parentheses.

**(** *EXPRESSION* **)**
     Returns the value of *EXPRESSION*.

Note that parentheses will usually require escaping with ``\`` (so they appear as ``\(`` and ``\)``) to avoid being interpreted as a command substitution.


Examples
--------

If the ``/tmp`` directory exists, copy the ``/etc/motd`` file to it:

::

    if test -d /tmp
        cp /etc/motd /tmp/motd
    end


If the variable :envvar:`MANPATH` is defined and not empty, print the contents. (If :envvar:`MANPATH` is not defined, then it will expand to zero arguments, unless quoted.)

::

    if test -n "$MANPATH"
        echo $MANPATH
    end

Be careful with unquoted variables::

    if test -n $MANPATH
        # This will also be reached if $MANPATH is unset,
        # because in that case we have `test -n`, so it checks if "-n" is non-empty, and it is.
        echo $MANPATH
    end

This will change in a future release of fish, or already with the ``test-require-arg`` :ref:`feature flag <featureflags>` - if $MANPATH is unset, ``if test -n $MANPATH`` will be false.

Parentheses and the ``-o`` and ``-a`` operators can be combined to produce more complicated expressions. In this example, success is printed if there is a ``/foo`` or ``/bar`` file as well as a ``/baz`` or ``/bat`` file.

::

    if test \( -f /foo -o -f /bar \) -a \( -f /baz -o -f /bat \)
        echo Success.
    end


Numerical comparisons will simply fail if one of the operands is not a number:

::

    if test 42 -eq "The answer to life, the universe and everything"
        echo So long and thanks for all the fish # will not be executed
    end

A common comparison is with :envvar:`status`:

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

Unlike many things in fish, ``test`` implements a subset of the `IEEE Std 1003.1-2008 (POSIX.1) standard <https://pubs.opengroup.org/onlinepubs/9699919799/utilities/test.html>`__. The following exceptions apply:

- The ``<`` and ``>`` operators for comparing strings are not implemented.
- With ``test-require-arg``, the zero- and one-argument modes will behave differently.

 In cases such as this, one can use ``command`` ``test`` to explicitly use the system's standalone ``test`` rather than this ``builtin`` ``test``.

See also
--------

Other commands that may be useful as a condition, and are often easier to use:

- :doc:`string`, which can do string operations including wildcard and regular expression matching
- :doc:`path`, which can do file checks and operations, including filters on multiple paths at once
