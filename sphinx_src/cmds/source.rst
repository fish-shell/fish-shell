source - evaluate contents of file.
==========================================

Synopsis
--------

source FILENAME [ARGUMENTS...]
somecommand | source


Description
------------

``source`` evaluates the commands of the specified file in the current shell. This is different from starting a new process to perform the commands (i.e. ``fish < FILENAME``) since the commands will be evaluated by the current shell, which means that changes in shell variables will affect the current shell. If additional arguments are specified after the file name, they will be inserted into the ``$argv`` variable. The ``$argv`` variable will not include the name of the sourced file.

If no file is specified and stdin is not the terminal, or if the file name '``-``' is used, stdin will be read.

The return status of ``source`` is the return status of the last job to execute. If something goes wrong while opening or reading the file, ``source`` exits with a non-zero status.

``.`` (a single period) is an alias for the ``source`` command. The use of ``.`` is deprecated in favour of ``source``, and ``.`` will be removed in a future version of fish.


Example
------------



::

    source ~/.config/fish/config.fish
    # Causes fish to re-read its initialization file.


\subsection Caveats

In fish versions prior to 2.3.0 the ``$argv`` variable would have a single element (the name of the sourced file) if no arguments are present. Otherwise it would contain arguments without the name of the sourced file. That behavior was very confusing and unlike other shells such as bash and zsh.
