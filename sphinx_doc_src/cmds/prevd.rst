.. _cmd-prevd:

prevd - move backward through directory history
===============================================

Synopsis
--------

::

    prevd [ -l | --list ] [POS]

Description
-----------

``prevd`` moves backwards ``POS`` positions in the history of visited directories; if the beginning of the history has been hit, a warning is printed.

If the ``-l`` or ``--list`` flag is specified, the current history is also displayed.

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

