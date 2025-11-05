string-shorten - shorten strings to a width, with an ellipsis
===============================================================

Synopsis
--------

.. BEGIN SYNOPSIS

.. synopsis::

    string shorten [(-c | --char) CHARS] [(-m | --max) INTEGER]
                   [-N | --no-newline] [-l | --left] [-q | --quiet] [STRING ...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string shorten`` truncates each *STRING* to the given visible width and adds an ellipsis to indicate it. "Visible width" means the width of all visible characters added together, excluding escape sequences and accounting for :envvar:`fish_emoji_width` and :envvar:`fish_ambiguous_width`. It is the amount of columns in a terminal the *STRING* occupies.

The escape sequences reflect what fish knows about, and how it computes its output. Your terminal might support more escapes, or not support escape sequences that fish knows about.

If **-m** or **--max** is given, truncate at the given width. Otherwise, the lowest non-zero width of all input strings is used. A max of 0 means no shortening takes place, all STRINGs are printed as-is.

If **-N** or **--no-newline** is given, only the first line (or last line with **--left**) of each STRING is used, and an ellipsis is added if it was multiline. This only works for STRINGs being given as arguments, multiple lines given on stdin will be interpreted as separate STRINGs instead.

If **-c** or **--char** is given, add *CHAR* instead of an ellipsis. This can also be empty or more than one character.

If **-l** or **--left** is given, remove text from the left on instead, so this prints the longest *suffix* of the string that fits. With **--no-newline**, this will take from the last line instead of the first.

If **-q** or **--quiet** is given, ``string shorten`` only runs for the return value - if anything would be shortened, it returns 0, else 1.

The default ellipsis is ``…``. If fish thinks your system is incapable because of your locale, it will use ``...`` instead.

The return value is 0 if any shortening occurred, 1 otherwise.

.. END DESCRIPTION

Examples
--------

.. BEGIN EXAMPLES

::

    >_ string shorten foo foobar
    # No width was given, we infer, and "foo" is the shortest.
    foo
    fo…

    >_ string shorten --char="..." foo foobar
    # The target width is 3 because of "foo",
    # and our ellipsis is 3 too, so we can't really show anything.
    # This is the default ellipsis if your locale doesn't allow "…".
    foo
    ...

    >_ string shorten --char="" --max 4 abcdef 123456
    # Leaving the char empty makes us not add an ellipsis
    # So this truncates at 4 columns:
    abcd
    1234

    >_ touch "a multiline"\n"file"
    >_ for file in *; string shorten -N -- $file; end
    # Shorten the multiline file so we only show one line per file:
    a multiline…

    >_ ss -p | string shorten -m$COLUMNS -c ""
    # `ss` from Linux' iproute2 shows socket information, but prints extremely long lines.
    # This shortens input so it fits on the screen without overflowing lines.

    >_ git branch | string match -rg '^\* (.*)' | string shorten -m20
    # Take the current git branch and shorten it at 20 columns.
    # Here the branch is "builtin-path-with-expand"
    builtin-path-with-e…

    >_ git branch | string match -rg '^\* (.*)' | string shorten -m20 --left
    # Taking 20 columns from the right instead:
    …in-path-with-expand

.. END EXAMPLES

See Also
--------

.. BEGIN SEEALSO

- :doc:`string pad <string-pad>` does the inverse of this command, adding padding to a specific width instead.
  
- The :doc:`printf <printf>` command can do simple padding, for example ``printf %10s\n`` works like ``string pad -w10``.

- :doc:`string length <string-length>` with the ``--visible`` option can be used to show what fish thinks the width is.
