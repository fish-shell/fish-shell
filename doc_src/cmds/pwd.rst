.. _cmd-pwd:

pwd - output the current working directory
==========================================

Synopsis
--------

::

    pwd [(-P | --physical)] [(-L | --logical)]


Description
-----------

``pwd`` outputs (prints) the current working directory.

The following options are available:

- ``-L`` or ``--logical`` Output the logical working directory, without resolving symlinks (default behavior).

- ``-P`` or ``--physical`` Output the physical working directory, with symlinks resolved.
