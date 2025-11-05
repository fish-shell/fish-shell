prevd - move backward through directory history
===============================================

Synopsis
--------

.. synopsis::

    prevd [-l | --list] [POS]

Description
-----------

``prevd`` moves backwards *POS* positions in the :ref:`history of visited directories <directory-history>`; if the beginning of the history has been hit, a warning is printed.

If the **-l** or **--list** flag is specified, the current history is also displayed.

Note that the ``cd`` command limits directory history to the 25 most recently visited directories. The history is stored in the :envvar:`dirprev` and :envvar:`dirnext` variables which this command manipulates.

The **-h** or **--help** option displays help about using this command.

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

See Also
--------

- the :doc:`cdh <cdh>` command to display a prompt to quickly navigate the history
- the :doc:`dirh <dirh>` command to print the directory history
- the :doc:`nextd <nextd>` command to move forward
