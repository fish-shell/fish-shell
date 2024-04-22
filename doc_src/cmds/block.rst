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

``block`` delays delivery of all events triggered by ``fish`` or the :doc:`emit <emit>`, thus delaying the execution of any function registered ``--on-event``, ``--on-process-exit``, ``--on-job-exit``, ``--on-variable`` and ``--on-signal`` until after the block is removed.

Event blocks should not be confused with code blocks, which are created with ``begin``, ``if``, ``while`` or ``for``

Without options, ``block`` sets up a block that is released automatically at the end of the current function scope.

The following options are available:

**-l** or **--local**
    Release the block automatically at the end of the current innermost code block scope.

**-g** or **--global**
    Never automatically release the lock.

**-e** or **--erase**
    Release global block.

**-h** or **--help**
    Display help about using this command.

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

Events are only received from the current fish process as there is no way to send events from one fish process to another.
