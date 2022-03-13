.. _cmd-dirs:

dirs - print directory stack
============================

Synopsis
--------

.. synopsis::

    dirs [-c]

Description
-----------

``dirs`` prints the current :ref:`directory stack <directory-stack>`, as created by :ref:`pushd <cmd-pushd>` and modified by :ref:`popd <cmd-popd>`.

The following options are available:

**-c**:
    Clear the directory stack instead of printing it.

**-h** or **--help**
    Displays help about using this command.

``dirs`` does not accept any arguments.

See Also
--------

- the :ref:`cdh <cmd-cdh>` command, which provides a more intuitive way to navigate to recently visited directories.
