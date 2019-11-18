string-repeat - multiply a string
=================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string repeat [(-n | --count) COUNT] [(-m | --max) MAX] [(-N | --no-newline)] [(-q | --quiet)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string repeat`` repeats the STRING ``-n`` or ``--count`` times. The ``-m`` or ``--max`` option will limit the number of outputted char (excluding the newline). This option can be used by itself or in conjunction with ``--count``. If both ``--count`` and ``--max`` are present, max char will be outputed unless the final repeated string size is less than max, in that case, the string will repeat until count has been reached. Both ``--count`` and ``--max`` will accept a number greater than or equal to zero, in the case of zero, nothing will be outputed. If ``-N`` or ``--no-newline`` is given, the output won't contain a newline character at the end. Exit status: 0 if yielded string is not empty, 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

Repeat Examples
^^^^^^^^^^^^^^^

::

    >_ string repeat -n 2 'foo '
    foo foo

    >_ echo foo | string repeat -n 2
    foofoo

    >_ string repeat -n 2 -m 5 'foo'
    foofo

    >_ string repeat -m 5 'foo'
    foofo

.. END EXAMPLES
