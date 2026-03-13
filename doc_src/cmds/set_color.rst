set_color - set the terminal color
==================================

Synopsis
--------

.. synopsis::

    set_color [OPTIONS] [VALUE]

Description
-----------

``set_color`` is used to control the color and styling of text in the terminal. *VALUE* describes that styling. *VALUE* can be a reserved color name like **red** or an RGB color value given as 3 or 6 hexadecimal digits ("F27" or "FF2277"). A special keyword **normal** resets text formatting to terminal defaults.

Valid colors include:

  - **black**, **red**, **green**, **yellow**, **blue**, **magenta**, **cyan**, **white**
  - **brblack**, **brred**, **brgreen**, **bryellow**, **brblue**, **brmagenta**, **brcyan**, **brwhite**

The *br*- (as in 'bright') forms are full-brightness variants of the 8 standard-brightness colors on many terminals. **brblack** has higher brightness than **black** - towards gray.

An RGB value with three or six hex digits, such as A0FF33 or f2f can be used.
A three digit value is equivalent to specifying each digit twice; e.g., ``set_color 2BC`` is the same as ``set_color 22BBCC``.
Hexadecimal RGB values can be in lower or uppercase.

If :envvar:`fish_term24bit` is set to 0, fish will translate RGB values to the nearest color on the 256-color palette.
If :envvar:`fish_term256` is also set to 0, fish will translate them to the 16-color palette instead.
Fish launched as ``fish -d term_support`` will include diagnostic messages that indicate the color support mode in use.

If multiple colors are specified, fish prefers the first RGB one.
However if :envvar:`fish_term256` is set to 0, fish prefers the first named color specified.

The following options are available:

**-f** or **--foreground** *COLOR*
    Sets the foreground color.
    This is equivalent to calling ``set_color COLOR`` with the exception that the keyword **normal** will only reset the foreground color to its default, instead of all colors and modes.
    It cannot be used with *VALUE* or **--print-colors**.

**-b** or **--background** *COLOR*
    Sets the background color.

**--underline-color** *COLOR*
    Set the underline color.

**-c** or **--print-colors**
    Prints the given colors or a colored list of the 16 named colors.
    It cannot be used with **--foreground**.

**-o** or **--bold**
    Sets bold mode.

**-d** or **--dim**
    Sets dim mode.

**-i** or **--italics**, or **-iSTATE** or **--italics=STATE**
    Sets italics mode. The state can be **on** (default), or **off**.

**-r** or **--reverse**, or **-iSTATE** or **--reverse=STATE**
    Sets reverse mode. The state can be **on** (default), or **off**.

**-s** or **--strikethrough**, or **-sSTATE** or **--strikethrough=STATE**
    Sets strikethrough mode. The state can be **on** (default), or **off**.

**-u** or **--underline**, or **-uSTYLE** or **--underline=STYLE**
    Set the underline mode; supported styles are **single** (default), **double**, **curly**, **dotted**, **dashed** and **off**.

**--reset**
    Reset the text formatting to the terminal defaults before applying the new colors and modes.
    This is equivalent to calling ``set_color normal`` except that it is possible to set the foreground color in the same call (e.g. ``set_color --reset green``)

**--theme=THEME**
    Ignored.
    :ref:`Color variables <variables-color>` that contain only this option are treated like missing / empty color variables,
    i.e. fish will use the fallback color instead.
    :doc:`fish_config theme choose <fish_config>` erases all :ref:`color variable <fish_config-color-variables>`
    whose value includes this option, and adds this option to all color variables it sets.
    This allows identifying variables set by a theme,
    and it allows fish to update color variables whenever :envvar:`fish_terminal_color_theme` changes.

**-h** or **--help**
    Displays help about using this command.

Notes
-----

1. Using ``set_color normal`` will reset all colors and modes to the terminal's default.
2. In contrast, ``set_color --foreground normal`` will only reset the foreground color and leave all the other colors and modes unchanged.
3. Setting the background color only affects subsequently written characters. Fish provides no way to set the background color for the entire terminal window. Configuring the window background color (and other attributes such as its opacity) has to be done using whatever mechanisms the terminal provides. Look for a config option.
4. Some terminals use the ``--bold`` escape sequence to switch to a brighter color set rather than increasing the weight of text.
5. ``set_color`` works by printing sequences of characters to standard output. If used in command substitution or a pipe, these characters will also be captured. This may or may not be desirable. Checking the exit status of ``isatty stdout`` before using ``set_color`` can be useful to decide not to colorize output in a script.

Examples
--------


::

    set_color red; echo "Roses are red"
    set_color blue; echo "Violets are blue"
    set_color 62A; echo "Eggplants are dark purple"
    set_color normal; echo "Normal is nice" # Resets the background too
