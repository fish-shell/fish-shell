.. _cmd-dirs:

dirs - print directory stack
============================

Synopsis
--------

::

    dirs
    dirs -c

Description
-----------

``dirs`` prints the current :ref:`directory stack <directory-stack>`, as created by :ref:`pushd <cmd-pushd>` and modified by :ref:`popd <cmd-popd>`.

With "-c", it clears the directory stack instead.

``dirs`` does not accept any parameters.

See Also
--------

- the :ref:`cdh <cmd-cdh>` command which provides a more intuitive way to navigate to recently visited directories.
