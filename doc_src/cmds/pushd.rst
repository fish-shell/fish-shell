pushd - push directory to directory stack
=========================================

Synopsis
--------

.. synopsis::

    pushd DIRECTORY

Description
-----------

The ``pushd`` function adds *DIRECTORY* to the top of the :ref:`directory stack <directory-stack>` and makes it the current working directory. :doc:`popd <popd>` will pop it off and return to the original directory.

Without arguments, it exchanges the top two directories in the stack.

``pushd +NUMBER`` rotates the stack counter-clockwise i.e. from bottom to top

``pushd -NUMBER`` rotates clockwise i.e. top to bottom.

The **-h** or **--help** option displays help about using this command.

Example
-------

::

    cd ~/dir1
    pushd ~/dir2
    pushd ~/dir3
    # Working directory is now ~/dir3
    # Directory stack contains ~/dir2 ~/dir1

    pushd /tmp
    # Working directory is now /tmp
    # Directory stack contains ~/dir3 ~/dir2 ~/dir1

    pushd +1
    # Working directory is now ~/dir3
    # Directory stack contains ~/dir2 ~/dir1 /tmp

    popd
    # Working directory is now ~/dir2
    # Directory stack contains ~/dir1 /tmp

See Also
--------

- the :doc:`dirs <dirs>` command to print the directory stack
- the :doc:`cdh <cdh>` command which provides a more intuitive way to navigate to recently visited directories.
