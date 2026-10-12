fish_user_key_bindings - add user-defined key bindings
=======================================================

Synopsis
--------

.. synopsis::

    fish_user_key_bindings

Description
-----------

``fish_user_key_bindings`` is called automatically whenever fish sets up the :ref:`key bindings <custom-bindings>`.

That allows you to use it as a simple customization point.

Historically, fish could not remember which bindings were defined by a preset like :doc:`fish_default_key_bindings <fish_default_key_bindings>` or by custom configuration, so ``fish_user_key_bindings`` was the only way to add custom bindings.

That hasn't been necessary for a while but it stays around because it makes for easy organization - just keep all bindings here to have them in one spot.

Adding :doc:`bind <bind>` invocations anywhere in :ref:`config.fish <configuration>` works too, so which you use is a question of taste.

Examples
--------

::

    function fish_user_key_bindings
        # I like the old feature of leaving a ^C marker on ctrl-c
        bind ctrl-c cancel-commandline
    end

