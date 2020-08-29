.. _cmd-fish_cmd_not_found:

fish_command_not_found - what to do when a command wasn't found
===============================================================

Synopsis
--------

::

  function fish_command_not_found
      ...
  end


Description
-----------

When fish can't find a command, it invokes this function.

It can print a message, usually tailored to the OS, to e.g. check which package would contain the missing command,
by calling out to a handler provided by the operating system.

Fish ships multiple handlers for various Linux distributions and chooses from them when this function is loaded,
or you can define your own.

It receives the full commandline as one argument per token, so $argv[1] contains the missing command.

When you leave ``fish_command_not_found`` undefined (e.g. by adding an empty function file) or explicitly call ``__fish_default_command_not_found_handler``, fish will just print a simple error.

Example
-------

A simple handler:

::

    function fish_command_not_found
        echo Did not find command $argv[1]
    end

    > flounder
    echo Did not find command flounder

Or the handler for OpenSUSE's command-not-found::

    function fish_command_not_found
        /usr/bin/command-not-found $argv[1]
    end

Or the simple default handler::

    function fish_command_not_found
        __fish_default_command_not_found_handler $argv
    end

Backwards compatibility
-----------------------

This command was introduced in fish 3.2.0. Previous versions of fish used the "fish_command_not_found" :ref:`event <event>` instead.

To define a handler that works in older versions of fish as well, define it the old way::

  function __fish_command_not_found_handler --on-event fish_command_not_found
       echo COMMAND WAS NOT FOUND MY FRIEND $argv[1]
  end

in which case fish will define a ``fish_command_not_found`` that calls it,
or define a wrapper::

  function fish_command_not_found
       echo "G'day mate, could not find your command: $argv"
  end

  function __fish_command_not_found_handler --on-event fish_command_not_found
       fish_command_not_found $argv
  end
