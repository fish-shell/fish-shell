string-pad - pad strings to a fixed width
=========================================

Synopsis
--------

.. BEGIN SYNOPSIS

.. synopsis::

    string pad [-r | --right] [-C | --center] [(-c | --char) CHAR] [(-w | --width) INTEGER]
               [STRING ...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string pad`` extends each *STRING* to the given visible width by adding *CHAR* to the left. That means the width of all visible characters added together, excluding escape sequences and accounting for :envvar:`fish_emoji_width` and :envvar:`fish_ambiguous_width`. It is the amount of columns in a terminal the *STRING* occupies.

The escape sequences reflect what fish knows about, and how it computes its output. Your terminal might support more escapes, or not support escape sequences that fish knows about.

If **-C** or **--center** is given, add the padding to before and after the string. If it is impossible to perfectly center the result (because the required amount of padding is an odd number), extra padding will be added to the left, unless **--right** is also given.

If **-r** or **--right** is given, add the padding after a string.

If **-c** or **--char** is given, pad with *CHAR* instead of whitespace.

The output is padded to the maximum width of all input strings. If **-w** or **--width** is given, use at least that.

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

.. END EXAMPLES

See Also
--------

.. BEGIN SEEALSO

- The :doc:`printf <printf>` command can do simple padding, for example ``printf %10s\n`` works like ``string pad -w10``.

- :doc:`string length <string-length>` with the ``--visible`` option can be used to show what fish thinks the width is.
