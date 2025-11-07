dirs - print directory stack
============================

Synopsis
--------

.. synopsis::

    dirs [-c]

Description
-----------

``dirs`` prints the current :ref:`directory stack <directory-stack>`, as created by :doc:`pushd <pushd>` and modified by :doc:`popd <popd>`.

The following options are available:

**-c**:
    Clear the directory stack instead of printing it.

**-h** or **--help**
    Displays help about using this command.

``dirs`` does not accept any arguments.

See Also
--------

- the :doc:`cdh <cdh>` command, which provides a more intuitive way to navigate to recently visited directories.
