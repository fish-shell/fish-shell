set_color - set the terminal color
==================================

Synopsis
--------

.. synopsis::

    set_color [OPTIONS] VALUE

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

**-b** or **--background** *COLOR*
    Sets the background color.

**--underline-color** *COLOR*
    Set the underline color.

**-c** or **--print-colors**
    Prints the given colors or a colored list of the 16 named colors.

**-o** or **--bold**
    Sets bold mode.

**-d** or **--dim**
    Sets dim mode.

**-i** or **--italics**
    Sets italics mode.

**-r** or **--reverse**
    Sets reverse mode.

**-u** or **--underline**, or **-uSTYLE** or **--underline=STYLE**
    Set the underline mode; supported styles are **single** (default), **double**, **curly**, **dotted** and **dashed**.

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

1. Using **set_color normal** will reset all colors and modes to the terminal's default.
2. Setting the background color only affects subsequently written characters. Fish provides no way to set the background color for the entire terminal window. Configuring the window background color (and other attributes such as its opacity) has to be done using whatever mechanisms the terminal provides. Look for a config option.
3. Some terminals use the ``--bold`` escape sequence to switch to a brighter color set rather than increasing the weight of text.
4. ``set_color`` works by printing sequences of characters to standard output. If used in command substitution or a pipe, these characters will also be captured. This may or may not be desirable. Checking the exit status of ``isatty stdout`` before using ``set_color`` can be useful to decide not to colorize output in a script.

Examples
--------


::

    set_color red; echo "Roses are red"
    set_color blue; echo "Violets are blue"
    set_color 62A; echo "Eggplants are dark purple"
    set_color normal; echo "Normal is nice" # Resets the background too
