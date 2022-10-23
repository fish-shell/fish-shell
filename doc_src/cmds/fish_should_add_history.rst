.. _cmd-fish_should_add_history:

fish_should_add_history - decide whether a command should be added to the history
==============================================================

Synopsis
--------

.. synopsis::

    fish_should_add_history

::

  function fish_should_add_history
      ...
  end


Description
-----------

The ``fish_should_add_history`` function is executed after every command that would normaly be saved to the disk. It is used to decide whether the command should be saved to the history or not.

The function is executed with the command as its first argument. If the function returns a status of 0, the command is saved to the history. If the function returns a status of 255, the command is not saved to the history.

The function can also return a status of 1 or 2, to save the command in ephemeral or memory mode respectively. This is useful for commands that should not be saved to the history file, but should be available for the up and down arrow keys.

Example
-------

A simple example:



::

    function fish_should_add_history
        for cmd in clear
        string match -qr "^$cmd" -- $argv; and return 255
        end
        return 0
    end


