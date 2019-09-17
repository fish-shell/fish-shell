.. _cmd-nextd:

nextd - move forward through directory history
==============================================

Synopsis
--------

::

    nextd [ -l | --list ] [POS]


Description
-----------

``nextd`` moves forwards ``POS`` positions in the history of visited directories; if the end of the history has been hit, a warning is printed.

If the ``-l`` or ``--list`` flag is specified, the current directory history is also displayed.

Note that the ``cd`` command limits directory history to the 25 most recently visited directories. The history is stored in the ``$dirprev`` and ``$dirnext`` variables which this command manipulates.

You may be interested in the :ref:`cdh <cmd-cdh>` command which provides a more intuitive way to navigate to recently visited directories.

Example
-------



::

    cd /usr/src
    # Working directory is now /usr/src

    cd /usr/src/fish-shell
    # Working directory is now /usr/src/fish-shell

    prevd
    # Working directory is now /usr/src

    nextd
    # Working directory is now /usr/src/fish-shell

