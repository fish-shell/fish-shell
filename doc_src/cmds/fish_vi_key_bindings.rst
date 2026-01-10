fish_vi_key_bindings - set vi key bindings for fish
===============================================================

Synopsis
--------

.. synopsis::

    fish_vi_key_bindings
    fish_vi_key_bindings [--no-erase] [INIT_MODE]

Description
-----------

``fish_vi_key_bindings`` sets the vi key bindings for ``fish`` shell.

If a valid *INIT_MODE* is provided (insert, default, visual), then that mode will become the default
. If no *INIT_MODE* is given, the mode defaults to insert mode.

The following parameters are available:

**--no-erase**
    Does not clear previous set bindings

Further information on how to use :ref:`vi mode <vi-mode>`.

Differences from Vim
--------------------

Fish's vi mode aims to be familiar to vim users, but there are some differences:

**Word character handling**
    In vim, underscore (``_``) is treated as a keyword character by default, so word motions like ``w``, ``b``, and ``e`` treat ``foo_bar`` as a single word. In fish, underscore is treated as punctuation, so word motions stop at underscores. For example, pressing ``w`` on ``foo_bar`` in fish stops at the ``_``, while in vim it would jump past the entire identifier.

**The** ``cw`` **command**
    In vim, ``cw`` has special behavior: when the cursor is on a non-space character, it behaves like ``ce`` (change to end of word), but when the cursor is on a space, it behaves like ``dwi`` (delete word then insert).

    In fish, ``cw`` always behaves like ``dwi`` - it deletes to the start of the next word (including trailing whitespace), then enters insert mode. To get vim's ``cw`` behavior in fish, use ``ce`` instead.

Examples
--------

To start using vi key bindings::

  fish_vi_key_bindings

or ``set -g fish_key_bindings fish_vi_key_bindings`` in :ref:`config.fish <configuration>`.
