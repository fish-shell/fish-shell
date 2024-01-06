.. _cmd-fish_greeting:

fish_greeting - display a welcome message in interactive shells
===============================================================

Synopsis
--------

.. synopsis::

    fish_greeting

::

  function fish_greeting
      ...
  end


Description
-----------

When an interactive fish starts, it executes fish_greeting and displays its output.

The default fish_greeting is a function that prints a variable of the same name (``$fish_greeting``), so you can also just change that if you just want to change the text.

While you could also just put ``echo`` calls into config.fish, fish_greeting takes care of only being used in interactive shells, so it won't be used e.g. with ``scp`` (which executes a shell), which prevents some errors.

Example
-------

To just empty the text, with the default greeting function::

  set -U fish_greeting

or ``set -g fish_greeting`` in :ref:`config.fish <configuration>`.

A simple greeting:

::

  function fish_greeting
      echo Hello friend!
      echo The time is (set_color yellow)(date +%T)(set_color normal) and this machine is called $hostname
  end
