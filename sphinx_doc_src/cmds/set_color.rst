.. _cmd-set_color:

set_color - set the terminal color
==================================

Synopsis
--------

::

    set_color [OPTIONS] VALUE

Description
-----------

``set_color`` is used to control the color and styling of text in the terminal. ``VALUE`` describes that styling. It's a reserved color name like *red* or a RGB color value given as 3 or 6 hexadecimal digits ("F27" or "FF2277"). A special keyword *normal* resets text formatting to terminal defaults.

Valid colors include:

  - *black*, *red*, *green*, *yellow*, *blue*, *magenta*, *cyan*, *white*
  - *brblack*, *brred*, *brgreen*, *bryellow*, *brblue*, *brmagenta*, *brcyan*, *brwhite*

The *br*- (as in 'bright') forms are full-brightness variants of the 8 standard-brightness colors on many terminals. *brblack* has higher brightness than *black* - towards gray.

An RGB value with three or six hex digits, such as A0FF33 or f2f can be used. ``fish`` will choose the closest supported color. A three digit value is equivalent to specifying each digit twice; e.g., ``set_color 2BC`` is the same as ``set_color 22BBCC``. Hexadecimal RGB values can be in lower or uppercase. Depending on the capabilities of your terminal (and the level of support ``set_color`` has for it) the actual color may be approximated by a nearby matching reserved color name or ``set_color`` may not have an effect on color.

A second color may be given as a desired fallback color. e.g. ``set_color 124212 brblue`` will instruct set_color to use *brblue* if a terminal is not capable of the exact shade of grey desired. This is very useful when an 8 or 16 color terminal might otherwise not use a color.

The following options are available:

- ``-b``, ``--background`` *COLOR* sets the background color.
- ``-c``, ``--print-colors`` prints a list of the 16 named colors.
- ``-o``, ``--bold`` sets bold mode.
- ``-d``, ``--dim`` sets dim mode.
- ``-i``, ``--italics`` sets italics mode.
- ``-r``, ``--reverse`` sets reverse mode.
- ``-u``, ``--underline`` sets underlined mode.

Using the *normal* keyword will reset foreground, background, and all formatting back to default.

Notes
-----

1. Using the *normal* keyword will reset both background and foreground colors to whatever is the default for the terminal.
2. Setting the background color only affects subsequently written characters. Fish provides no way to set the background color for the entire terminal window. Configuring the window background color (and other attributes such as its opacity) has to be done using whatever mechanisms the terminal provides. Look for a config option.
3. Some terminals use the ``--bold`` escape sequence to switch to a brighter color set rather than increasing the weight of text.
4. ``set_color`` works by printing sequences of characters to *stdout*. If used in command substitution or a pipe, these characters will also be captured. This may or may not be desirable. Checking the exit status of ``isatty stdout`` before using ``set_color`` can be useful to decide not to colorize output in a script.

Examples
--------


::

    set_color red; echo "Roses are red"
    set_color blue; echo "Violets are blue"
    set_color 62A; echo "Eggplants are dark purple"
    set_color normal; echo "Normal is nice" # Resets the background too


Terminal Capability Detection
-----------------------------

Fish uses some heuristics to determine what colors a terminal supports to avoid sending sequences that it won't understand.

In particular it will:

- Enable 256 colors if $TERM contains "xterm", except for known exceptions (like MacOS 10.6 Terminal.app)
- Enable 24-bit ("true-color") even if the $TERM entry only reports 256 colors. This includes modern xterm, VTE-based terminals like Gnome Terminal, Konsole and iTerm2.
- Detect support for italics, dim, reverse and other modes.

If terminfo reports 256 color support for a terminal, 256 color support will always be enabled.

To force true-color support on or off, set $fish_term24bit to "1" for on and 0 for off - ``set -g fish_term24bit 1``.

To debug color palette problems, ``tput colors`` may be useful to see the number of colors in terminfo for a terminal. Fish launched as ``fish -d2`` will include diagnostic messages that indicate the color support mode in use.

The ``set_color`` command uses the terminfo database to look up how to change terminal colors on whatever terminal is in use. Some systems have old and incomplete terminfo databases, and lack color information for terminals that support it. Fish assumes that all terminals can use the [ANSI X3.64](https://en.wikipedia.org/wiki/ANSI_escape_code) escape sequences if the terminfo definition indicates a color below 16 is not supported.

