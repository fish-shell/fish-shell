.. _cmd-fish_vi_key_bindings:

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

Examples
--------

To start using vi key bindings::

  fish_vi_key_bindings

or ``set -g fish_key_bindings fish_vi_key_bindings`` in :ref:`config.fish <configuration>`.
