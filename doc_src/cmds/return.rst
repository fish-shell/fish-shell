return - stop the current inner function
========================================

Synopsis
--------

.. synopsis::

    return [N]

Description
-----------

:program:`return` halts a currently running function.

It is often added inside of a conditional block such as an :doc:`if <if>` statement or a :doc:`switch <switch>` statement to conditionally stop the executing function and return to the caller; it can also be used to specify the exit status of a function.

If invoked outside of a function by a non-interactive shell, or inside :doc:`source <source>`, it is equivalent to :doc:`exit <exit>`, meaning it it will stop execution of the current script.

If invoked outside of a function by an interactive shell shell, execution will continue as normal.

The **-h** or **--help** option displays help about using this command.

Exit status
-----------

*N* if given, the previous exit status (:envvar:`status`) otherwise.

Example
-------

An implementation of the false command as a fish function:
::

    function false
        return 1
    end
