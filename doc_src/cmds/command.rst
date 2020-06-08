.. _cmd-command:

command - run a program
=======================

Synopsis
--------

::

    command [OPTIONS] COMMANDNAME [ARGS...]

Description
-----------

``command`` forces the shell to execute the program ``COMMANDNAME`` and ignore any functions or builtins with the same name.

The following options are available:

- ``-a`` or ``--all`` returns all the external COMMANDNAMEs that are found in ``$PATH`` in the order they are found.

- ``-q`` or ``--quiet``, silences the output and prints nothing, setting only the exit status. Implies ``--search``.

- ``-s`` or ``--search`` returns the name of the external command that would be executed, or nothing if no file with the specified name could be found in the ``$PATH``.

With the ``-s`` option, ``command`` treats every argument as a separate command to look up and sets the exit status to 0 if any of the specified commands were found, or 1 if no commands could be found. Additionally passing a ``-q`` or ``--quiet`` option prevents any paths from being printed, like ``type -q``, for testing only the exit status.

For basic compatibility with POSIX ``command``, the ``-v`` flag is recognized as an alias for ``-s``.

Examples
--------

``command ls`` causes fish to execute the ``ls`` program, even if an ``ls`` function exists.

``command -s ls`` returns the path to the ``ls`` program.

``command -q git; and command git log`` runs ``git log`` only if ``git`` exists.
