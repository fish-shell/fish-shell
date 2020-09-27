string-pad - pad characters before and after string
===================================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string pad [(-r | --right)] [(-c | --char) CHAR] [(-w | --width) INTEGER] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string pad`` pads each STRING with CHAR to the given width.

The default behavior is left padding with spaces and default width is the length of string (hence, no padding).

If ``-r`` or ``--right`` is given, only pad after string.

The ``-c`` or ``--char`` switch causes padding with the character CHAR instead of default whitespace character.

If ``-w`` or ``--width`` is given, pad the string to given width. Width less than the string width will result in an unchanged string.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string pad -w 10 -c ' ' 'abc'
           abc

    >_ string pad --right --width 12 --char=z foo barbaz
    foozzzzzzzzz
    barbazzzzzzz

    >_ string pad -w 6 --char=- foo | string pad --right -w 9 --char=-
    ---foo---


.. END EXAMPLES
