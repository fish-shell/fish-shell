.. _cmd-realpath:

realpath - Convert a path to an absolute path without symlinks
==============================================================

Synopsis
--------

::

    realpath PATH

Description
-----------

``realpath`` resolves a path to its absolute path.

fish provides a ``realpath`` builtin as a fallback for systems where there is no ``realpath`` command. fish's implementation always resolves its first argument, and does not support any options.

If the operation fails, an error will be reported.
