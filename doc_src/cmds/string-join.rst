string-join - join strings with delimiter
=========================================

Synopsis
--------

.. BEGIN SYNOPSIS

.. synopsis::

    string join [-q | --quiet] SEP [STRING ...]
    string join0 [-q | --quiet] [STRING ...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string join`` joins its *STRING* arguments into a single string separated by *SEP*, which can be an empty string. Exit status: 0 if at least one join was performed, or 1 otherwise.

``string join0`` joins its *STRING* arguments into a single string separated by the zero byte (NUL), and adds a trailing NUL. This is most useful in conjunction with tools that accept NUL-delimited input, such as ``sort -z``. Exit status: 0 if at least one join was performed, or 1 otherwise.

Because Unix uses NUL as the string terminator, passing the output of ``string join0`` as an *argument* to a command (via a :ref:`command substitution <expand-command-substitution>`) won't actually work. Fish will pass the correct bytes along, but the command won't be able to tell where the argument ends. This is a limitation of Unix' argument passing.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ seq 3 | string join ...
    1...2...3

    # Give a list of NUL-separated filenames to du (this is a GNU extension)
    >_ string join0 file1 file2 file\nwith\nmultiple\nlines | du --files0-from=-

    # Just put the strings together without a separator
    >_ string join '' a b c
    abc

.. END EXAMPLES
