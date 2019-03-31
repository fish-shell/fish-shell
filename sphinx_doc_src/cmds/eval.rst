.. _cmd-eval:

eval - evaluate the specified commands
======================================

Synopsis
--------

eval [COMMANDS...]


Description
-----------
``eval`` evaluates the specified parameters as a command. If more than one parameter is specified, all parameters will be joined using a space character as a separator.

If your command does not need access to stdin, consider using ``source`` instead.

Example
-------

The following code will call the ls command. Note that ``fish`` does not support the use of shell variables as direct commands; ``eval`` can be used to work around this.



::

    set cmd ls
    eval $cmd


