builtin - run a builtin command
===============================

Synopsis
--------

.. synopsis::

    builtin [OPTIONS] BUILTINNAME
    builtin --query BUILTINNAME ...
    builtin --names

Description
-----------

``builtin`` forces the shell to use a builtin command named *BUILTIN*, rather than a function or external program.

The following options are available:

**-n** or **--names**
    Lists the names of all defined builtins.

**-q** or **--query** *BUILTIN*
    Tests if any of the specified builtins exist. If any exist, it returns 0, 1 otherwise.

**-h** or **--help**
    Displays help about using this command.

Example
-------

::

    builtin jobs
    # executes the jobs builtin, even if a function named jobs exists

