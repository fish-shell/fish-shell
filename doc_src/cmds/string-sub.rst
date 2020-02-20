string-sub - extract substrings
===============================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string sub [(-s | --start) START] [(-l | --length) LENGTH] [(-q | --quiet)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string sub`` prints a substring of each string argument. The start of the substring can be specified with ``-s`` or ``--start`` followed by a 1-based index value. Positive index values are relative to the start of the string and negative index values are relative to the end of the string. The default start value is 1. The length of the substring can be specified with ``-l`` or ``--length``. If the length is not specified, the substring continues to the end of each STRING. Exit status: 0 if at least one substring operation was performed, 1 otherwise.

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

.. END EXAMPLES
