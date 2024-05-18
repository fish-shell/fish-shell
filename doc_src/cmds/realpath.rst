.. _cmd-realpath:
.. program::realpath

realpath - convert a path to an absolute path without symlinks
==============================================================

Synopsis
--------

.. synopsis::

    realpath [OPTIONS] PATH

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``realpath``.
          To see the documentation on any non-fish versions, use ``command man realpath``.

:program:`realpath` follows all symbolic links encountered for the provided :envvar:`PATH`, printing the absolute path resolved. :doc:`fish <fish>` provides a :command:`realpath`-alike builtin intended to enrich systems where no such command is installed by default.

If a :command:`realpath` command exists, that will be preferred.
``builtin realpath`` will explicitly use the fish implementation of :command:`realpath`.

The following options are available:

**-s** or **--no-symlinks**
    Don't resolve symlinks, only make paths absolute, squash multiple slashes and remove trailing slashes.

**-h** or **--help**
    Displays help about using this command.
