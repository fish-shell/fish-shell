fish_should_add_to_history - decide whether a command should be added to the history
====================================================================================

Synopsis
--------

.. synopsis::

    fish_should_add_to_history

::

  function fish_should_add_to_history
      ...
  end


Description
-----------

The ``fish_should_add_to_history`` function is executed before fish adds a command to history, and its return status decides whether that is done.

If it returns 0, the command is stored in history, when it returns anything else, it is not. In the latter case the command can still be recalled for one command.

The first argument to ``fish_should_add_to_history`` is the commandline. History is added *before* a command is run, so e.g. :envvar:`status` can't be checked. This is so commands that don't finish like :doc:`exec` and long-running commands are available in new sessions immediately.

If ``fish_should_add_to_history`` doesn't exist, fish will save a command to history unless it starts with a space. If it does exist, this function takes over all of the duties, so commands starting with space are saved unless ``fish_should_add_to_history`` says otherwise.

Example
-------

A simple example:

::

    function fish_should_add_to_history
        for cmd in vault mysql ls
             string match -qr "^$cmd" -- $argv; and return 1
        end
        return 0
    end

This refuses to store any immediate "vault", "mysql" or "ls" calls. Commands starting with space would be stored.

::

    function fish_should_add_to_history
        # I don't want `git pull`s in my history when I'm in a specific repository
        if string match -qr '^git pull' -- "$argv"
        and string match -qr "^/home/me/my-secret-project/" -- (pwd -P)
            return 1
        end

        return 0
    end
