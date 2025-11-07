eval - evaluate the specified commands
======================================

Synopsis
--------

.. synopsis::

    eval [COMMANDS ...]

Description
-----------
**eval** evaluates the specified parameters as a command.
If more than one parameter is specified, all parameters will be joined using a space character as a separator.

If the command does not need access to stdin, consider using :doc:`source <source>` instead.

If no piping or other compound shell constructs are required, variable-expansion-as-command, as in  ``set cmd ls -la; $cmd``, is also an option.

Example
-------

The following code will call the ls command and truncate each filename to the first 12 characters.

::

    set cmd ls \| cut -c 1-12
    eval $cmd

