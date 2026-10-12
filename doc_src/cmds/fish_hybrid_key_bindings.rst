fish_hybrid_key_bindings - set vi key bindings for fish, with emacs bindings on top
=====================================================================================

Synopsis
--------

.. synopsis::

    fish_hybrid_key_bindings

Description
-----------

``fish_hybrid_key_bindings`` enables vi-style key bindings, but keeps the emacs-style bindings that don't conflict.

For instance, ctrl-b and ctrl-f move the cursor back and forward one character, respectively. This is a very emacs-like binding, so with the usual vi-bindings it would be rmoved so fish behaves more like vi. But some prefer to keep them around, so this is a quick way to do it.

See also :ref:`vi mode <vi-mode>` and :doc:`fish_vi_key_bindings <fish_vi_key_bindings>`.

Examples
--------

To start using hybrid key bindings::

  fish_hybrid_key_bindings

or ``set -g fish_key_bindings fish_hybrid_key_bindings`` in :ref:`config.fish <configuration>`.
