.. _cmd-source:

source - evaluate contents of file
==================================

Synopsis
--------

.. synopsis::

    source FILE [ARGUMENTS ...]
    SOMECOMMAND | source
    . FILE [ARGUMENTS ...]


Description
-----------

``source`` evaluates the commands of the specified *FILE* in the current shell as a new block of code. This is different from starting a new process to perform the commands (i.e. ``fish < FILE``) since the commands will be evaluated by the current shell, which means that changes in shell variables will affect the current shell. If additional arguments are specified after the file name, they will be inserted into the :envvar:`argv` variable. The :envvar:`argv` variable will not include the name of the sourced file.

fish will search the working directory to resolve relative paths but will not search :envvar:`PATH` .

If no file is specified and stdin is not the terminal, or if the file name ``-`` is used, stdin will be read.

The exit status of ``source`` is the exit status of the last job to execute. If something goes wrong while opening or reading the file, ``source`` exits with a non-zero status.

Some other shells only support the **.** alias (a single period).
The use of **.** is deprecated in favour of ``source``, and **.** will be removed in a future version of fish.

``source`` creates a new :ref:`local scope<variables-scope>`; ``set --local`` within a sourced block will not affect variables in the enclosing scope.

The **-h** or **--help** option displays help about using this command.

Example
-------



::

    source ~/.config/fish/config.fish
    # Causes fish to re-read its initialization file.


Caveats
-------

In fish versions prior to 2.3.0, the :envvar:`argv` variable would have a single element (the name of the sourced file) if no arguments are present. Otherwise, it would contain arguments without the name of the sourced file. That behavior was very confusing and unlike other shells such as bash and zsh.
