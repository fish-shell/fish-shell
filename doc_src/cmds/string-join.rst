string-join - join strings with delimiter
=========================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string join [(-l | --left)] [(-r | --right)] [(-q | --quiet)] SEP [STRING...]
    string join0 [(-q | --quiet)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string join`` joins its STRING arguments into a single string separated by SEP, which can be an empty string. If ``-l`` or ``--left`` is given, SEP will be prepended to each STRING argument. If ``-r`` or ``--right`` is given, SEP will be appended to each STRING argument. Exit status: 0 if at least one join was performed, or 1 otherwise.

``string join0`` joins its STRING arguments into a single string separated by the zero byte (NUL), and adds a trailing NUL. This is most useful in conjunction with tools that accept NUL-delimited input, such as ``sort -z``. Exit status: 0 if at least one join was performed, or 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ seq 3 | string join ...
    1...2...3

    >_ seq 3 | string join --left ...
    ...1...2...3

    >_ seq 3 | string join --right ...
    1...2...3...

    >_ seq 3 | string join -l ... -r ...
    ...1...2...3...

.. END EXAMPLES
