end - end a block of commands
=============================

Synopsis
--------

.. synopsis::

    begin
        [COMMANDS ...] 
    end

.. synopsis::

    function NAME [OPTIONS]; COMMANDS ...; end
    if CONDITION; COMMANDS_TRUE ...; [else; COMMANDS_FALSE ...;] end
    switch VALUE; [case [WILDCARD ...]; [COMMANDS ...]; ...] end
    while CONDITION; COMMANDS ...; end
    for VARNAME in [VALUES ...]; COMMANDS ...; end

Description
-----------

The **end** keyword ends a block of commands started by one of the following commands:

- :doc:`begin <begin>` to start a block of commands
- :doc:`function <function>` to define a function
- :doc:`if <if>`, :doc:`switch <switch>` to conditionally execute commands
- :doc:`while <while>`, :doc:`for <for>` to perform commands multiple times

The **end** keyword does not change the current exit status.
Instead, the status after it will be the status returned by the most recent command.
