string-length - print string lengths
====================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string length [(-q | --quiet)] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string length`` reports the length of each string argument in characters. Exit status: 0 if at least one non-empty STRING was given, or 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string length 'hello, world'
    12

    >_ set str foo
    >_ string length -q $str; echo $status
    0
    # Equivalent to test -n $str

.. END EXAMPLES
