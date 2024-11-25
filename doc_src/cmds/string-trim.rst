string-trim - remove trailing whitespace
========================================

Synopsis
--------

.. BEGIN SYNOPSIS

.. synopsis::

    string trim [-l | --left] [-r | --right] [(-c | --chars) CHARS]
                [-q | --quiet] [STRING ...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string trim`` removes leading and trailing whitespace from each *STRING*. If **-l** or **--left** is given, only leading whitespace is removed. If **-r** or **--right** is given, only trailing whitespace is trimmed.

The **-c** or **--chars** switch causes the set of characters in *CHARS* to be removed instead of whitespace. This is a set of characters, not a string - if you pass ``-c foo``, it will remove any "f" or "o", not just "foo" as a whole.

Exit status: 0 if at least one character was trimmed, or 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string trim ' abc  '
    abc

    >_ string trim --right --chars=yz xyzzy zany
    x
    zan


.. END EXAMPLES
