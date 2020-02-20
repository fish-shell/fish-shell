.. _cmd-exit:

exit - exit the shell
=====================

Synopsis
--------

::

    exit [STATUS]

Description
-----------

``exit`` causes fish to exit. If ``STATUS`` is supplied, it will be converted to an integer and used as the exit status. Otherwise, the exit status will be that of the last command executed.

If exit is called while sourcing a file (using the :ref:`source <cmd-source>` builtin) the rest of the file will be skipped, but the shell itself will not exit.
