.. _cmd-return:

return - stop the current inner function
========================================

Synopsis
--------

::

    function NAME; [COMMANDS...;] return [STATUS]; [COMMANDS...;] end

Description
-----------

``return`` halts a currently running function. The exit status is set to ``STATUS`` if it is given.

It is usually added inside of a conditional block such as an :ref:`if <cmd-if>` statement or a :ref:`switch <cmd-switch>` statement to conditionally stop the executing function and return to the caller, but it can also be used to specify the exit status of a function.


Example
-------

The following code is an implementation of the false command as a fish function



::

    function false
        return 1
    end



