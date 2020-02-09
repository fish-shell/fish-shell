.. _cmd-cd:

cd - change directory
=====================

Synopsis
--------

::

    cd [DIRECTORY]

Description
-----------
``cd`` changes the current working directory.

If ``DIRECTORY`` is supplied, it will become the new directory. If no parameter is given, the contents of the ``HOME`` environment variable will be used.

If ``DIRECTORY`` is a relative path, the paths found in the ``CDPATH`` list will be tried as prefixes for the specified path, in addition to $PWD.

Note that the shell will attempt to change directory without requiring ``cd`` if the name of a directory is provided (starting with ``.``, ``/`` or ``~``, or ending with ``/``).

Fish also ships a wrapper function around the builtin ``cd`` that understands ``cd -`` as changing to the previous directory. See also :ref:`prevd <cmd-prevd>`. This wrapper function maintains a history of the 25 most recently visited directories in the ``$dirprev`` and ``$dirnext`` global variables. If you make those universal variables your ``cd`` history is shared among all fish instances.

As a special case, ``cd .`` is equivalent to ``cd $PWD``, which is useful in cases where a mountpoint has been recycled or a directory has been removed and recreated.

Examples
--------



::

    cd
    # changes the working directory to your home directory.
    
    cd /usr/src/fish-shell
    # changes the working directory to /usr/src/fish-shell


See Also
--------

See also the :ref:`cdh <cmd-cdh>` command for changing to a recently visited directory.
