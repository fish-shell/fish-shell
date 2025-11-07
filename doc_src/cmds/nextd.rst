nextd - move forward through directory history
==============================================

Synopsis
--------

.. synopsis::

    nextd [-l | --list] [POS]

Description
-----------

``nextd`` moves forwards *POS* positions in the :ref:`history of visited directories <directory-history>`; if the end of the history has been hit, a warning is printed.

If the **-l** or **--list** option is specified, the current directory history is also displayed.

The **-h** or **--help** option displays help about using this command.

Note that the ``cd`` command limits directory history to the 25 most recently visited directories. The history is stored in the :envvar:`dirprev` and :envvar:`dirnext` variables which this command manipulates.

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
- the :doc:`prevd <prevd>` command to move backward
