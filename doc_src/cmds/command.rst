.. _cmd-command:

command - run a program
=======================

Synopsis
--------

.. synopsis::

    command [OPTIONS] [COMMANDNAME [ARG ...]]

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``command``.
          To see the documentation on any non-fish versions, use ``command man command``.

**command** forces the shell to execute the program *COMMANDNAME* and ignore any functions or builtins with the same name.

In ``command foo``, ``command`` is a keyword.

The following options are available:

**-a** or **--all**
    Prints all *COMMAND* found in :envvar:`PATH`, in the order found.

**-q** or **--query**
    Return 0 if any of the given commands could be found, 127 otherwise.
    Don't print anything.
    For compatibility, this is also **--quiet** (deprecated).

**-s** or **--search** (or **-v**)
    Prints the external command that would be executed, or prints nothing if no file with the specified name could be found in :envvar:`PATH`.

**-h** or **--help**
    Displays help about using this command.

Examples
--------

| ``command ls`` executes the ``ls`` program, even if an ``ls`` function also exists.
| ``command -s ls`` prints the path to the ``ls`` program.
| ``command -q git; and command git log`` runs ``git log`` only if ``git`` exists.
| ``command -sq git`` and ``command -q git`` and ``command -vq git`` return true (0) if a git command could be found and don't print anything.
