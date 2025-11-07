type - locate a command and describe its type
=============================================

Synopsis
--------

.. synopsis::

    type [OPTIONS] NAME [...]

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``type``.
          To see the documentation on any non-fish versions, use ``command man type``.

With no options, :command:`type` indicates how each *NAME* would be interpreted if used as a command name.

The following options are available:

**-a** or **--all**
    Prints all of possible definitions of the specified names.

**-s** or **--short**
    Don't print function definitions when used with no options or with **-a**/**--all**.

**-f** or **--no-functions**
    Suppresses function lookup.

**-t** or **--type**
    Prints ``function``, ``builtin``, or ``file`` if *NAME* is a shell function, builtin, or disk file, respectively.

**-p** or **--path**
    Prints the path to *NAME* if *NAME* resolves to an executable file in :envvar:`PATH`, the path to the script containing the definition of the function *NAME* if *NAME* resolves to a function loaded from a file on disk (i.e. not interactively defined at the prompt), or nothing otherwise.

**-P** or **--force-path**
    Returns the path to the executable file *NAME*, presuming *NAME* is found in the :envvar:`PATH` environment variable, or nothing otherwise. **--force-path** explicitly resolves only the path to executable files in  :envvar:`PATH`, regardless of whether *NAME* is shadowed by a function or builtin with the same name.

**-q** or **--query**
    Suppresses all output; this is useful when testing the exit status. For compatibility with old fish versions this is also **--quiet**.

**-h** or **--help**
    Displays help about using this command.

The **-q**, **-p**, **-t** and **-P** flags (and their long flag aliases) are mutually exclusive. Only one can be specified at a time.

``type`` returns 0 if at least one entry was found, 1 otherwise, and 2 for invalid options or option combinations.

Example
-------

::

    >_ type fg
    fg is a builtin

