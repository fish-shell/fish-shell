isatty - test if a file descriptor is a tty.
==========================================

Synopsis
--------

isatty [FILE DESCRIPTOR]


Description
------------

`isatty` tests if a file descriptor is a tty.

`FILE DESCRIPTOR` may be either the number of a file descriptor, or one of the strings `stdin`, `stdout`, or `stderr`.

If the specified file descriptor is a tty, the exit status of the command is zero. Otherwise, the exit status is non-zero. No messages are printed to standard error.


Examples
------------

From an interactive shell, the commands below exit with a return value of zero:



::

    isatty
    isatty stdout
    isatty 2
    echo | isatty 1


And these will exit non-zero:



::

    echo | isatty
    isatty 9
    isatty stdout > file
    isatty 2 2> file

