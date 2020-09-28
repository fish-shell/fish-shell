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

    >_ string pad -w 10 abc abcdef
           abc
        abcdef

    >_ string pad --right --char=🐟 "fish are pretty" "rich. "
    fish are pretty
    rich.  🐟🐟🐟🐟

    >_ string pad -w$COLUMNS (date)
    # Prints the current time on the right edge of the screen.



See Also
--------

- The :ref:`printf <cmd-printf>` command can do simple padding, for example ``printf %10s\n`` works like ``string pad -w10``.

.. END EXAMPLES
