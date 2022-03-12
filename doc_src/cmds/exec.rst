.. _cmd-exec:

exec - execute command in current process
=========================================

Synopsis
--------

.. synopsis::

    exec COMMAND

Description
-----------

``exec`` replaces the currently running shell with a new command. On successful completion, ``exec`` never returns. ``exec`` cannot be used inside a pipeline.

The **--help** or **-h** option displays help about using this command.

Example
-------

``exec emacs`` starts up the emacs text editor, and exits ``fish``. When emacs exits, the session will terminate.
