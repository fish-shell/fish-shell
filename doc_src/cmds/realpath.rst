.. program::realpath

realpath - convert a path to an absolute path without symlinks
==============================================================

Synopsis
--------

**realpath** [*options*] *PATH*

Description
-----------

:program:`realpath` follows all symbolic links encountered for the provided :envvar:`PATH`, printing the absolute path resolved. :program:`fish` provides a :command:`realpath`-alike builtin intended to be enrich systems where no such command is installed by default.

If a :command:`realpath` command exists, that will be preferred.
``builtin realpath`` will explicitly use the fish implementation of :command:`realpath`.

The following options are available:

- ``-s`` or ``--no-symlinks``: Don't resolve symlinks, only make paths absolute, squash multiple slashes and remove trailing slashes.
