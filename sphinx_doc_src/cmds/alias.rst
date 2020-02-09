.. _cmd-alias:

alias - create a function
=========================

Synopsis
--------

::

    alias
    alias [OPTIONS] NAME DEFINITION
    alias [OPTIONS] NAME=DEFINITION


Description
-----------

``alias`` is a simple wrapper for the ``function`` builtin, which creates a function wrapping a command. It has similar syntax to POSIX shell ``alias``. For other uses, it is recommended to define a :ref:`function <cmd-function>`.

``fish`` marks functions that have been created by ``alias`` by including the command used to create them in the function description. You can list ``alias``-created functions by running ``alias`` without arguments. They must be erased using ``functions -e``.

- ``NAME`` is the name of the alias
- ``DEFINITION`` is the actual command to execute. The string ``$argv`` will be appended.

You cannot create an alias to a function with the same name. Note that spaces need to be escaped in the call to ``alias`` just like at the command line, *even inside quoted parts*.

The following options are available:

- ``-h`` or ``--help`` displays help about using this command.

- ``-s`` or ``--save`` Automatically save the function created by the alias into your fish configuration directory using :ref:`funcsave <cmd-funcsave>`.

Example
-------

The following code will create ``rmi``, which runs ``rm`` with additional arguments on every invocation.



::

    alias rmi="rm -i"
    
    # This is equivalent to entering the following function:
    function rmi --wraps rm --description 'alias rmi=rm -i'
        rm -i $argv
    end
    
    # This needs to have the spaces escaped or "Chrome.app..." will be seen as an argument to "/Applications/Google":
    alias chrome='/Applications/Google\ Chrome.app/Contents/MacOS/Google\ Chrome banana'

