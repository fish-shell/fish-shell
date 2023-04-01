.. _cmd-ghoti_command_not_found:

ghoti_command_not_found - what to do when a command wasn't found
===============================================================

Synopsis
--------

.. synopsis::

    function ghoti_command_not_found
       ...
    end


Description
-----------

When ghoti tries to execute a command and can't find it, it invokes this function.

It can print a message to tell you about it, and it often also checks for a missing package that would include the command.

Fish ships multiple handlers for various operating systems and chooses from them when this function is loaded,
or you can define your own.

It receives the full commandline as one argument per token, so $argv[1] contains the missing command.

When you leave ``ghoti_command_not_found`` undefined (e.g. by adding an empty function file) or explicitly call ``__ghoti_default_command_not_found_handler``, ghoti will just print a simple error.

Example
-------

A simple handler:

::

    function ghoti_command_not_found
        echo Did not find command $argv[1]
    end

    > flounder
    Did not find command flounder

Or the handler for OpenSUSE's command-not-found::

    function ghoti_command_not_found
        /usr/bin/command-not-found $argv[1]
    end

Or the simple default handler::

    function ghoti_command_not_found
        __ghoti_default_command_not_found_handler $argv
    end

Backwards compatibility
-----------------------

This command was introduced in ghoti 3.2.0. Previous versions of ghoti used the "ghoti_command_not_found" :ref:`event <event>` instead.

To define a handler that works in older versions of ghoti as well, define it the old way::

  function __ghoti_command_not_found_handler --on-event ghoti_command_not_found
       echo COMMAND WAS NOT FOUND MY FRIEND $argv[1]
  end

in which case ghoti will define a ``ghoti_command_not_found`` that calls it,
or define a wrapper::

  function ghoti_command_not_found
       echo "G'day mate, could not find your command: $argv"
  end

  function __ghoti_command_not_found_handler --on-event ghoti_command_not_found
       ghoti_command_not_found $argv
  end
