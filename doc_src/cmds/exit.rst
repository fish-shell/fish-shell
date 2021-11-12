.. _cmd-exit:
.. program::exit

exit - exit the shell
=====================

Synopsis
--------

**exit** [*code*]

Description
-----------

``exit`` is a special builtin that causes the shell to exit. Either 255 or the *code* supplied is used, whichever is lesser.
Otherwise, the exit status will be that of the last command executed.

If exit is called while sourcing a file (using the :ref:`source <cmd-source>` builtin) the rest of the file will be skipped, but the shell itself will not exit.
