.. _cmd-end:

end - end a block of commands.
==============================

Synopsis
--------

::

    begin; [COMMANDS...] end
    if CONDITION; COMMANDS_TRUE...; [else; COMMANDS_FALSE...;] end
    while CONDITION; COMMANDS...; end
    for VARNAME in [VALUES...]; COMMANDS...; end
    switch VALUE; [case [WILDCARD...]; [COMMANDS...]; ...] end

Description
-----------

``end`` ends a block of commands.

For more information, read the
documentation for the block constructs, such as ``if``, ``for`` and ``while``.

The ``end`` command does not change the current exit status. Instead, the status after it will be the status returned by the most recent command.
