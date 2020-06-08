.. _cmd-popd:

popd - move through directory stack
===================================

Synopsis
--------

::

    popd

Description
-----------

``popd`` removes the top directory from the :ref:`directory stack <directory-stack>` and changes the working directory to the new top directory. Use :ref:`pushd <cmd-pushd>` to add directories to the stack.

Example
-------

::

    pushd /usr/src
    # Working directory is now /usr/src
    # Directory stack contains /usr/src

    pushd /usr/src/fish-shell
    # Working directory is now /usr/src/fish-shell
    # Directory stack contains /usr/src /usr/src/fish-shell

    popd
    # Working directory is now /usr/src
    # Directory stack contains /usr/src

See Also
--------

- the :ref:`dirs <cmd-dirs>` command to print the directory stack
- the :ref:`cdh <cmd-cdh>` command which provides a more intuitive way to navigate to recently visited directories.
