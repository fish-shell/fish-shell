.. _cmd-pushd:

pushd - push directory to directory stack
=========================================

Synopsis
--------

::

    pushd [DIRECTORY]

Description
-----------

The ``pushd`` function adds ``DIRECTORY`` to the top of the directory stack and makes it the current working directory. :ref:`popd <cmd-popd>` will pop it off and return to the original directory.

Without arguments, it exchanges the top two directories in the stack.

``pushd +NUMBER`` rotates the stack counter-clockwise i.e. from bottom to top

``pushd -NUMBER`` rotates clockwise i.e. top to bottom.

See also :ref:`dirs <cmd-dirs>` to print the stack and ``dirs -c`` to clear it.

You may be interested in the :ref:`cdh <cmd-cdh>` command which provides a more intuitive way to navigate to recently visited directories.

Example
-------

::

    pushd /usr/src
    # Working directory is now /usr/src
    # Directory stack contains /usr/src

    pushd /usr/src/fish-shell
    # Working directory is now /usr/src/fish-shell
    # Directory stack contains /usr/src /usr/src/fish-shell

    pushd /tmp/
    # Working directory is now /tmp
    # Directory stack contains /tmp /usr/src /usr/src/fish-shell

    pushd +1
    # Working directory is now /usr/src
    # Directory stack contains /usr/src /usr/src/fish-shell /tmp

    popd
    # Working directory is now /usr/src/fish-shell
    # Directory stack contains /usr/src/fish-shell /tmp
