string-pad - pad strings to a fixed width
=========================================

Synopsis
--------

.. BEGIN SYNOPSIS

::

    string pad [(-r | --right)] [(-c | --char) CHAR] [(-w | --width) INTEGER] [STRING...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string pad`` extends each STRING to the given width by adding CHAR to the left.

If ``-r`` or ``--right`` is given, add the padding after a string.

If ``-c`` or ``--char`` is given, pad with CHAR instead of whitespace.

The output is padded to the maximum width of all input strings. If ``-w`` or ``--width`` is given, use at least that.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string pad -w 10 abc
           abc

    >_ string pad --right --char=🐟 "fish are pretty" "rich. "
    fish are pretty
    rich.  🐟🐟🐟🐟

    >_ string pad -w 6 -c- " | " "|||" " | " | string pad -r -w 9 -c-
    --- | ---
    ---|||---
    --- | ---


.. END EXAMPLES
