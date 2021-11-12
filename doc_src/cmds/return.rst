.. _cmd-return:
.. program::return:

return - stop the current inner function
========================================

Synopsis
--------

**return** [*n*]

Description
-----------

:program:`return` halts a currently running function.
The exit status is set to ``n`` if it is given.
If :program:`return` is invoked outside of a function or dot script it is equivalent to exit.

It is often added inside of a conditional block such as an :ref:`if <cmd-if>` statement or a :ref:`switch <cmd-switch>` statement to conditionally stop the executing function and return to the caller; it can also be used to specify the exit status of a function.

If at the top level of a script, it exits with the given status, like :ref:`exit <cmd-exit>`.
If at the top level in an interactive session, it will set :envvar:`status`, but not exit the shell.

Example
-------

An implementation of the false command as a fish function:
::

    function false
        return 1
    end
