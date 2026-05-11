fish_command_not_found - what to do when a command wasn't found
===============================================================

Synopsis
--------

.. synopsis::

    function fish_command_not_found
       ...
    end


Description
-----------

When fish tries to execute a command and can't find it, it invokes this function.

It can print a message to tell you about it, and it often also checks for a missing package that would include the command.

Fish ships multiple handlers for various operating systems and chooses from them when this function is loaded,
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
    Did not find command flounder

Or the handler for OpenSUSE's command-not-found::

    function fish_command_not_found
        /usr/bin/command-not-found $argv[1]
    end

Or the simple default handler::

    function fish_command_not_found
        __fish_default_command_not_found_handler $argv
    end
