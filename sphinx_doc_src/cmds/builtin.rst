builtin - run a builtin command
==========================================

Synopsis
--------

builtin BUILTINNAME [OPTIONS...]


Description
------------

`builtin` forces the shell to use a builtin command, rather than a function or program.

The following parameters are available:

- `-n` or `--names` List the names of all defined builtins


Example
------------



::

    builtin jobs
    # executes the jobs builtin, even if a function named jobs exists

