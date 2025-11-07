string-length - print string lengths
====================================

Synopsis
--------

.. BEGIN SYNOPSIS

.. synopsis::

    string length [-q | --quiet] [-V | --visible] [STRING ...]

.. END SYNOPSIS

Description
-----------

.. BEGIN DESCRIPTION

``string length`` reports the length of each string argument in characters. Exit status: 0 if at least one non-empty *STRING* was given, or 1 otherwise.

With **-V** or **--visible**, it uses the visible width of the arguments. That means it will discount escape sequences fish knows about, account for $fish_emoji_width and $fish_ambiguous_width. It will also count each line (separated by ``\n``) on its own, and with a carriage return (``\r``) count only the widest stretch on a line. The intent is to measure the number of columns the *STRING* would occupy in the current terminal.

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
    # Equivalent to test -n "$str"

    >_ string length --visible (set_color red)foobar
    # the set_color is discounted, so this is the width of "foobar"
    6

    >_ string length --visible 🐟🐟🐟🐟
    # depending on $fish_emoji_width, this is either 4 or 8
    # in new terminals it should be
    8
    
    >_ string length --visible abcdef\r123
    # this displays as "123def", so the width is 6
    6

    >_ string length --visible a\nbc
    # counts "a" and "bc" as separate lines, so it prints width for each
    1
    2

.. END EXAMPLES
