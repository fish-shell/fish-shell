.. _cmd-ghoti_greeting:

ghoti_greeting - display a welcome message in interactive shells
===============================================================

Synopsis
--------

.. synopsis::

    ghoti_greeting

::

  function ghoti_greeting
      ...
  end


Description
-----------

When an interactive ghoti starts, it executes ghoti_greeting and displays its output.

The default ghoti_greeting is a function that prints a variable of the same name (``$ghoti_greeting``), so you can also just change that if you just want to change the text.

While you could also just put ``echo`` calls into config.ghoti, ghoti_greeting takes care of only being used in interactive shells, so it won't be used e.g. with ``scp`` (which executes a shell), which prevents some errors.

Example
-------

To just empty the text, with the default greeting function::

  set -U ghoti_greeting

or ``set -g ghoti_greeting`` in :ref:`config.ghoti <configuration>`.

A simple greeting:

::

  function ghoti_greeting
      echo Hello friend!
      echo The time is (set_color yellow; date +%T; set_color normal) and this machine is called $hostname
  end
