.. _cmd-builtin:

builtin - run a builtin command
===============================

Synopsis
--------

::

    builtin [OPTIONS...] BUILTINNAME
    builtin --query BUILTINNAMES...

Description
-----------

``builtin`` forces the shell to use a builtin command, rather than a function or program.

The following parameters are available:

- ``-n`` or ``--names`` List the names of all defined builtins
- ``-q`` or ``--query`` tests if any of the specified builtins exists


Example
-------



::

    builtin jobs
    # executes the jobs builtin, even if a function named jobs exists

