.. SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
.. SPDX-FileCopyrightText: © 2009 fish-shell contributors
.. SPDX-FileCopyrightText: © 2022 fish-shell contributors
..
.. SPDX-License-Identifier: GPL-2.0-only

.. _cmd-block:

block - temporarily block delivery of events
============================================

Synopsis
--------

.. synopsis::

    block [(--local | --global)]
    block --erase

Description
-----------

``block`` prevents events triggered by ``fish`` or the :doc:`emit <emit>` command from being delivered and acted upon while the block is in place.

In functions, ``block`` can be useful while performing work that should not be interrupted by the shell.

The block can be removed. Any events which triggered while the block was in place will then be delivered.

Event blocks should not be confused with code blocks, which are created with ``begin``, ``if``, ``while`` or ``for``

Without options, the ``block`` command acts with function scope.

The following options are available:

**-l** or **--local**
    Release the block automatically at the end of the current innermost code block scope.

**-g** or **--global**
    Never automatically release the lock.

**-e** or **--erase**
    Release global block.

**-h** or **--help**
    Displays help about using this command.

Example
-------
::

    # Create a function that listens for events
    function --on-event foo foo; echo 'foo fired'; end

    # Block the delivery of events
    block -g

    emit foo
    # No output will be produced

    block -e
    # 'foo fired' will now be printed

Notes
-----

Events are only received from the current fish process as there is no way to send events from one fish process to another (yet).
