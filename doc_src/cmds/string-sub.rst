string-sub - extract substrings
===============================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string sub [(-s | --start) START] [(-e | --end) END] [(-l | --length) LENGTH] [(-q | --quiet)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string sub`` prints a substring of each string argument. The start/end of the substring can be specified with ``-s``/``-e`` or ``--start``/``--end`` followed by a 1-based index value. Positive index values are relative to the start of the string and negative index values are relative to the end of the string. The default start value is 1. The length of the substring can be specified with ``-l`` or ``--length``. If the length or end is not specified, the substring continues to the end of each STRING. Exit status: 0 if at least one substring operation was performed, 1 otherwise. ``--length`` is mutually exclusive with ``--end``.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string sub --length 2 abcde
    ab

    >_ string sub -s 2 -l 2 abcde
    bc

    >_ string sub --start=-2 abcde
    de

    >_ string sub --end=3 abcde
    abc

    >_ string sub -e -1 abcde
    abcd

    >_ string sub -s 2 -e -1 abcde
    bcd

    >_ string sub -s -3 -e -2 abcde
    c

.. END EXAMPLES
