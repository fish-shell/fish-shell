.. _cmd-pwd:

pwd - output the current working directory
==========================================

Synopsis
--------

.. synopsis::

    pwd [-P | --physical]
    pwd [-L | --logical]


Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``pwd``.
          To see the documentation on any non-fish versions, use ``command man pwd``.

``pwd`` outputs (prints) the current working directory.

The following options are available:

**-L** or **--logical**
    Output the logical working directory, without resolving symlinks (default behavior).

**-P** or **--physical**
    Output the physical working directory, with symlinks resolved.

**-h** or **--help**
    Displays help about using this command.

See Also
--------

Navigate directories using the :ref:`directory history <directory-history>` or the :ref:`directory stack <directory-stack>`
