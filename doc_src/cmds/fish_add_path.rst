.. _cmd-fish_add_path:
.. program::fish_add_path

fish_add_path - add to the path
==============================================================

Synopsis
--------

.. synopsis::

    fish_add_path path ...
    fish_add_path [(-g | --global) | (-U | --universal) | (-P | --path)] [(-m | --move)] [(-a | --append) | (-p | --prepend)] [(-v | --verbose) | (-n | --dry-run)] PATHS ...


Description
-----------

:program:`fish_add_path` is a simple way to add more components to fish's :envvar:`PATH`. It does this by adding the components either to $fish_user_paths or directly to :envvar:`PATH` (if the ``--path`` switch is given).

It is (by default) safe to use :program:`fish_add_path` in config.fish, or it can be used once, interactively, and the paths will stay in future because of :ref:`universal variables <variables-universal>`. This is a "do what I mean" style command, if you need more control, consider modifying the variable yourself.

Components are normalized by :doc:`realpath <realpath>`. Trailing slashes are ignored and relative paths are made absolute (but symlinks are not resolved). If a component already exists, it is not added again and stays in the same place unless the ``--move`` switch is given.

Components are added in the order they are given, and they are prepended to the path unless ``--append`` is given (if $fish_user_paths is used, that means they are last in $fish_user_paths, which is itself prepended to :envvar:`PATH`, so they still stay ahead of the system paths).

If no component is new, the variable (:envvar:`fish_user_paths` or :envvar:`PATH`) is not set again or otherwise modified, so variable handlers are not triggered.

If a component is not an existing directory, ``fish_add_path`` ignores it.

Options
-------

**-a** or **--append**
    Add components to the *end* of the variable.

**-p** or **--prepend**
    Add components to the *front* of the variable (this is the default).

**-g** or **--global**
    Use a global :envvar:`fish_user_paths`.

**-U** or **--universal**
    Use a universal :envvar:`fish_user_paths` - this is the default if it doesn't already exist.

**-P** or **--path**
    Manipulate :envvar:`PATH` directly.

**-m** or **--move**
    Move already-existing components to the place they would be added - by default they would be left in place and not added again.

**-v** or **--verbose**
    Print the :doc:`set <set>` command used.

**-n** or **--dry-run**
    Print the ``set`` command that would be used without executing it.

**-h** or **--help**
    Displays help about using this command.

If ``--move`` is used, it may of course lead to the path swapping order, so you should be careful doing that in config.fish.


Example
-------


::

   # I just installed mycoolthing and need to add it to the path to use it.
   > fish_add_path /opt/mycoolthing/bin

   # I want my ~/.local/bin to be checked first.
   > fish_add_path -m ~/.local/bin

   # I prefer using a global fish_user_paths
   > fish_add_path -g ~/.local/bin ~/.otherbin /usr/local/sbin

   # I want to append to the entire $PATH because this directory contains fallbacks
   > fish_add_path -aP /opt/fallback/bin

   # I want to add the bin/ directory of my current $PWD (say /home/nemo/)
   > fish_add_path -v bin/
   set fish_user_paths /home/nemo/bin /usr/bin /home/nemo/.local/bin

   # I have installed ruby via homebrew
   > fish_add_path /usr/local/opt/ruby/bin
