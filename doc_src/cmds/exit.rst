exit - exit the shell
=====================

Synopsis
--------

.. synopsis::

    exit [CODE]

Description
-----------

**exit** is a special builtin that causes the shell to exit. Either 255 or the *CODE* supplied is used, whichever is lesser.
Otherwise, the exit status will be that of the last command executed.

If exit is called while sourcing a file (using the :doc:`source <source>` builtin) the rest of the file will be skipped, but the shell itself will not exit.

The **--help** or **-h** option displays help about using this command.
