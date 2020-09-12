.. _cmd-realpath:

realpath - convert a path to an absolute path without symlinks
==============================================================

Synopsis
--------

::

    realpath PATH

Description
-----------

``realpath`` resolves a path to its absolute path.

fish provides a ``realpath`` builtin as a fallback for systems where there is no ``realpath`` command, your OS might provide a version with more features.

If a ``realpath`` command exists, it will be preferred, so if you want to use the builtin you should use ``builtin realpath`` explicitly.

The following options are available:

- ``-s`` or ``--no-symlinks``: Don't resolve symlinks, only make paths absolute, squash multiple slashes and remove trailing slashes.
