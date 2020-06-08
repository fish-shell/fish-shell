.. _cmd-type:

type - indicate how a command would be interpreted
==================================================

Synopsis
--------

::

    type [OPTIONS] NAME [NAME ...]


Description
-----------

With no options, ``type`` indicates how each ``NAME`` would be interpreted if used as a command name.

The following options are available:

- ``-a`` or ``--all`` prints all of possible definitions of the specified names.

- ``-s`` or ``--short`` suppresses function expansion when used with no options or with ``-a``/``--all``.

- ``-f`` or ``--no-functions`` suppresses function and builtin lookup.

- ``-t`` or ``--type`` prints ``function``, ``builtin``, or ``file`` if ``NAME`` is a shell function, builtin, or disk file, respectively.

- ``-p`` or ``--path`` prints the path to ``NAME`` if ``NAME`` resolves to an executable file in :ref:`$PATH <PATH>`, the path to the script containing the definition of the function ``NAME`` if ``NAME`` resolves to a function loaded from a file on disk (i.e. not interactively defined at the prompt), or nothing otherwise.

- ``-P`` or ``--force-path`` returns the path to the executable file ``NAME``, presuming ``NAME`` is found in ``$PATH``, or nothing otherwise. ``--force-path`` explicitly resolves only the path to executable files in ``$PATH``, regardless of whether ``$NAME`` is shadowed by a function or builtin with the same name.

- ``-q`` or ``--quiet`` suppresses all output; this is useful when testing the exit status.

The ``-q``, ``-p``, ``-t`` and ``-P`` flags (and their long flag aliases) are mutually exclusive. Only one can be specified at a time.


Example
-------



::

    >_ type fg
    fg is a builtin

