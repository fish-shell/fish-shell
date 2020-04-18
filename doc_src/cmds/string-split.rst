string-split - split strings by delimiter
=========================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string split [(-f | --fields) FIELDS] [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] SEP [STRING...]
    string split0 [(-f | --fields) FIELDS] [(-m | --max) MAX] [(-n | --no-empty)] [(-q | --quiet)] [(-r | --right)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string split`` splits each STRING on the separator SEP, which can be an empty string. If ``-m`` or ``--max`` is specified, at most MAX splits are done on each STRING. If ``-r`` or ``--right`` is given, splitting is performed right-to-left. This is useful in combination with ``-m`` or ``--max``. With ``-n`` or ``--no-empty``, empty results are excluded from consideration (e.g. ``hello\n\nworld`` would expand to two strings and not three). Exit status: 0 if at least one split was performed, or 1 otherwise.

Use ``-f`` or ``--fields`` to print out specific fields. Unless ``--allow-empty` is used, if a given field does not exist, then the command exits with status 1 and does not print anything.

See also the ``--delimiter`` option of the :ref:`read <cmd-read>` command.

``string split0`` splits each STRING on the zero byte (NUL). Options are the same as ``string split`` except that no separator is given.

``split0`` has the important property that its output is not further split when used in a command substitution, allowing for the command substitution to produce elements containing newlines. This is most useful when used with Unix tools that produce zero bytes, such as ``find -print0`` or ``sort -z``. See split0 examples below.


.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string split . example.com
    example
    com

    >_ string split -r -m1 / /usr/local/bin/fish
    /usr/local/bin
    fish

    >_ string split '' abc
    a
    b
    c

    >_ string split --allow-empty -f1,3,5 '' abc
    a
    c


NUL Delimited Examples
^^^^^^^^^^^^^^^^^^^^^^

::

    >_ # Count files in a directory, without being confused by newlines.
    >_ count (find . -print0 | string split0)
    42

    >_ # Sort a list of elements which may contain newlines
    >_ set foo beta alpha\\ngamma
    >_ set foo (string join0 $foo | sort -z | string split0)
    >_ string escape $foo[1]
    alpha\\ngamma

.. END EXAMPLES
