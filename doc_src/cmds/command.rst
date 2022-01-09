.. _cmd-command:

command - run a program
=======================

Synopsis
--------

**command** [**OPTIONS**] [*COMMANDNAME* [ARG ...]]

Description
-----------

**command** forces the shell to execute the program *COMMANDNAME* and ignore any functions or builtins with the same name.

The following options are available:

**-a** or **--all**
    Prints all *COMMAND* found in :envvar:`PATH`, in the order found.

**-q** or **--query**
    Silence output and print nothing, setting only exit status.
    Implies **--search**.
    For compatibility, this is also **--quiet** (deprecated).

**-v** (or **-s** or **--search**)
    Prints the external command that would be executed, or prints nothing if no file with the specified name could be found in :envvar:`PATH`.

With the **-v** option, ``command`` treats every argument as a separate command to look up and sets the exit status to 0 if any of the specified commands were found, or 127 if no commands could be found. **--quiet** used with **-v** prevents commands being printed, like ``type -q``.

Examples
--------

| ``command ls`` executes the ``ls`` program, even if an ``ls`` function also exists.
| ``command -s ls`` prints the path to the ``ls`` program.
| ``command -q git; and command git log`` runs ``git log`` only if ``git`` exists.
