alias - create a function
=========================

Synopsis
--------

.. synopsis::

    alias
    alias [--save] NAME DEFINITION
    alias [--save] NAME=DEFINITION


Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``alias``.
          To see the documentation on any non-fish versions, use ``command man alias``.

``alias`` is a simple wrapper for the ``function`` builtin, which creates a function wrapping a command. It has similar syntax to POSIX shell ``alias``. For other uses, it is recommended to define a :doc:`function <function>`.

If you want to ease your interactive use, to save typing, consider using an :doc:`abbreviation <abbr>` instead.

``fish`` marks functions that have been created by ``alias`` by including the command used to create them in the function description. You can list ``alias``-created functions by running ``alias`` without arguments. They must be erased using ``functions -e``.

- ``NAME`` is the name of the alias
- ``DEFINITION`` is the actual command to execute. ``alias`` automatically appends ``$argv``, so that all parameters used with the alias are passed to the actual command.

You cannot create an alias to a function with the same name. Note that spaces need to be escaped in the call to ``alias`` just like at the command line, *even inside quoted parts*.

The following options are available:

**-h** or **--help**
    Displays help about using this command.

**-s** or **--save**
    Saves the function created by the alias into your fish configuration directory using :doc:`funcsave <funcsave>`.

Example
-------

The following code will create ``rmi``, which runs ``rm`` with additional arguments on every invocation.

::

    alias rmi="rm -i"

    # This is equivalent to entering the following function:
    function rmi --wraps rm --description 'alias rmi=rm -i'
        rm -i $argv
    end

``alias`` sometimes requires escaping, as you can see here::

    # This needs to have the spaces escaped or "Chrome.app..."
    # will be seen as an argument to "/Applications/Google":
    alias chrome='/Applications/Google\ Chrome.app/Contents/MacOS/Google\ Chrome'


See more
--------

1. The :doc:`function <function>` command this builds on.
2. :ref:`Functions <syntax-function>`.
3. :ref:`Defining aliases <syntax-aliases>`.
