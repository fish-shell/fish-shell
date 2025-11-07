return - stop the current inner function
========================================

Synopsis
--------

.. synopsis::

    return [N]

Description
-----------

:program:`return` halts a currently running function.
The exit status is set to *N* if it is given.
If :program:`return` is invoked outside of a function or dot script it is equivalent to exit.

It is often added inside of a conditional block such as an :doc:`if <if>` statement or a :doc:`switch <switch>` statement to conditionally stop the executing function and return to the caller; it can also be used to specify the exit status of a function.

If at the top level of a script, it exits with the given status, like :doc:`exit <exit>`.
If at the top level in an interactive session, it will set :envvar:`status`, but not exit the shell.

The **-h** or **--help** option displays help about using this command.

Example
-------

An implementation of the false command as a fish function:
::

    function false
        return 1
    end
