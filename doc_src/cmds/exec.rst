.. SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
.. SPDX-FileCopyrightText: © 2009 fish-shell contributors
.. SPDX-FileCopyrightText: © 2022 fish-shell contributors
..
.. SPDX-License-Identifier: GPL-2.0-only

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
