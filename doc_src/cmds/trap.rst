trap - perform an action when the shell receives a signal
=========================================================

Synopsis
--------

.. synopsis::

    trap [OPTIONS] [[ARG] REASON ... ]

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``trap``.
          To see the documentation on any non-fish versions, use ``command man trap``.

``trap`` is a wrapper around the fish event delivery framework. It exists for backwards compatibility with POSIX shells. For other uses, it is recommended to define an :ref:`event handler <event>`.

The following parameters are available:

*ARG*
    Command to be executed on signal delivery.

*REASON*
    Name of the event to trap. For example, a signal like ``INT`` or ``SIGINT``, or the special symbol ``EXIT``.

**-l** or **--list-signals**
    Prints a list of signal names.

**-p** or **--print**
    Prints all defined signal handlers.

**-h** or **--help**
    Displays help about using this command.

If *ARG* and *REASON* are both specified, *ARG* is the command to be executed when the event specified by *REASON* occurs (e.g., the signal is delivered).

If *ARG* is absent (and there is a single *REASON*) or ``-``, each specified signal is reset to its original disposition (the value it had upon entrance to the shell).  If *ARG* is the null string the signal specified by each *REASON* is ignored by the shell and by the commands it invokes.

If *ARG* is not present and **-p** has been supplied, then the trap commands associated with each *REASON* are displayed. If no arguments are supplied or if only **-p** is given, ``trap`` prints the list of commands associated with each signal.

Signal names are case insensitive and the ``SIG`` prefix is optional. Trapping a signal will prevent fish from exiting in response to that signal.

The exit status is 1 if any *REASON* is invalid; otherwise trap returns 0.

Example
-------



::

    trap "status --print-stack-trace" SIGUSR1
    # Prints a stack trace each time the SIGUSR1 signal is sent to the shell.

