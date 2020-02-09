.. _cmd-psub:

psub - perform process substitution
===================================

Synopsis
--------

::

    COMMAND1 ( COMMAND2 | psub [-F | --fifo] [-f | --file] [-s SUFFIX])

Description
-----------

Some shells (e.g., ksh, bash) feature a syntax that is a mix between command substitution and piping, called process substitution. It is used to send the output of a command into the calling command, much like command substitution, but with the difference that the output is not sent through commandline arguments but through a named pipe, with the filename of the named pipe sent as an argument to the calling program. ``psub`` combined with a regular command substitution provides the same functionality.

The following options are available:

- ``-f`` or ``--file`` will cause psub to use a regular file instead of a named pipe to communicate with the calling process. This will cause ``psub`` to be significantly slower when large amounts of data are involved, but has the advantage that the reading process can seek in the stream. This is the default.

- ``-F`` or ``--fifo`` will cause psub to use a named pipe rather than a file. You should only use this if the command produces no more than 8 KiB of output. The limit on the amount of data a FIFO can buffer varies with the OS but is typically 8 KiB, 16 KiB or 64 KiB. If you use this option and the command on the left of the psub pipeline produces more output a deadlock is likely to occur.

- ``-s`` or ``--suffix`` will append SUFFIX to the filename.

Example
-------

::

    diff (sort a.txt | psub) (sort b.txt | psub)
    # shows the difference between the sorted versions of files ``a.txt`` and ``b.txt``.

    source-highlight -f esc (cpp main.c | psub -f -s .c)
    # highlights ``main.c`` after preprocessing as a C source.

