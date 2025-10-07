cd - change directory
=====================

Synopsis
--------

.. synopsis::

    cd [( -L | --no-dereference ) | ( -P | --dereference )] [DIRECTORY]

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``cd``.
          To see the documentation on any non-fish versions, use ``command man cd``.

``cd`` changes the current working directory.

The :envvar:`PWD` environment variable is updated with the new working directory, and the previous directory
is added to the :ref:`directory history <directory-history>`.

If *DIRECTORY* is given, it will become the new directory. If no parameter is given, the :envvar:`HOME` environment variable will be used.

If *DIRECTORY* is a relative path, all the paths in the :envvar:`CDPATH` will be tried as prefixes for it, in addition to :envvar:`PWD`.
It is recommended to keep **.** as the first element of :envvar:`CDPATH`, or :envvar:`PWD` will be tried last.

The new directory name is partially resolved to remove redundant segments (``.`` or ``..``).

``cd`` defaults to treating symbolic links as real directories, and not resolving them to their underlying
targets. The ``$PWD`` :ref:`special variable <variables-special>` variable will contain the path that was
supplied. This default behaviour can be enforced with the ``-L`` or ``--no-dereference`` option.

The ``-P`` or ``--dereference`` option resolves all symbolic links first. This was the default in fish versions before 3.0.0.

Fish will also try to change directory if given a command that looks like a directory (starting with **.**, **/** or **~**, or ending with **/**), without explicitly requiring **cd**.

Fish also ships a wrapper function around the builtin **cd** that understands ``cd -`` as changing to the previous directory.
See also :doc:`prevd <prevd>`.
This wrapper function maintains a history of the 25 most recently visited directories in the ``$dirprev`` and ``$dirnext`` global variables.
If you make those universal variables your **cd** history is shared among all fish instances.

As a special case, ``cd .`` is equivalent to ``cd $PWD``, which is useful in cases where a mountpoint has been recycled or a directory has been removed and recreated.

The **--help** or **-h** option displays help about using this command, and does not change the directory.

Examples
--------

::

    cd
    # changes the working directory to your home directory.

    cd /usr/src/fish-shell
    # changes the working directory to /usr/src/fish-shell

    cd -P /tmp/link
    # resolves /tmp/link to its target before recording the directory

See Also
--------

Navigate directories using the :ref:`directory history <directory-history>` or the :ref:`directory stack <directory-stack>`
