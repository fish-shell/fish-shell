\section set_color set_color - set the terminal color

\subsection set_color-synopsis Synopsis
\fish{synopsis}
set_color [OPTIONS] VALUE
\endfish

\subsection set_color-description Description

`set_color` is used to control the color and styling of text in the terminal. `VALUE` corresponds to a reserved color name such as *red* or a RGB color value given as 3 or 6 hexadecimal digits. The *br*-, as in 'bright', forms are full-brightness variants of the 8 standard-brightness colors on many terminals. *brblack* has higher brightness than *black* - towards gray. A special keyword *normal* resets text formatting to terminal defaults.

Valid colors include:

  - *black*, *red*, *green*, *yellow*, *blue*, *magenta*, *cyan*, *white*
  - *brblack*, *brred*, *brgreen*, *bryellow*, *brblue*, *brmagenta*, *brcyan*, *brwhite*

An RGB value with three or six hex digits, such as A0FF33 or f2f can be used. `fish` will choose the closest supported color. A three digit value is equivalent to specifying each digit twice; e.g., `set_color 2BC` is the same as `set_color 22BBCC`. Hexadecimal RGB values can be in lower or uppercase. Depending on the capabilities of your terminal (and the level of support `set_color` has for it) the actual color may be approximated by a nearby matching reserved color name or `set_color` may not have an effect on color. A second color may be given as a desired fallback color. e.g. `set_color 124212` *brblue* will instruct set_color to use *brblue* if a terminal is not capable of the exact shade of grey desired. This is very useful when an 8 or 16 color terminal might otherwise not use a color.

The following options are available:

- `-b`, `--background` *COLOR* sets the background color.
- `-c`, `--print-colors` prints a list of the 16 named colors.
- `-o`, `--bold` sets bold mode.
- `-d`, `--dim` sets dim mode.
- `-i`, `--italics` sets italics mode.
- `-r`, `--reverse` sets reverse mode.
- `-u`, `--underline` sets underlined mode.

Using the *normal* keyword will reset foreground, background, and all formatting back to default.

\subsection set_color-notes Notes

1. Using the *normal* keyword will reset both background and foreground colors to whatever is the default for the terminal.
2. Setting the background color only affects subsequently written characters. Fish provides no way to set the background color for the entire terminal window. Configuring the window background color (and other attributes such as its opacity) has to be done using whatever mechanisms the terminal provides.
3. Some terminals use the `--bold` escape sequence to switch to a brighter color set rather than increasing the weight of text.
4. `set_color` works by printing sequences of characters to *stdout*. If used in command substitution or a pipe, these characters will also be captured. This may or may not be desirable. Checking the exit code of `isatty stdout` before using `set_color` can be useful to decide not to colorize output in a script.

\subsection set_color-example Examples

\fish
set_color red; echo "Roses are red"
set_color blue; echo "Violets are blue"
set_color 62A; echo "Eggplants are dark purple"
set_color normal; echo "Normal is nice" # Resets the background too
\endfish

\subsection set_color-detection Terminal Capability Detection

Fish uses a heuristic to decide if a terminal supports the 256-color palette as opposed to the more limited 16 color palette of older terminals. Support can be forced on by setting `fish_term256` to *1*. If `$TERM` contains "256color" (e.g., *xterm-256color*), 256-color support is enabled. If `$TERM` contains *xterm*, 256 color support is enabled (except for MacOS: `$TERM_PROGRAM` and `$TERM_PROGRAM_VERSION` are used to detect Terminal.app from MacOS 10.6; support is disabled here it because it is known that it reports `xterm` and only supports 16 colors.

If terminfo reports 256 color support for a terminal, support will always be enabled. To debug color palette problems, `tput colors` may be useful to see the number of colors in terminfo for a terminal. Fish launched as `fish -d2` will include diagnostic messages that indicate the color support mode in use.

Many terminals support 24-bit (i.e., true-color) color escape sequences. This includes modern xterm, Gnome Terminal, Konsole, and iTerm2. Fish attempts to detect such terminals through various means in `config.fish` You can explicitly force that support via `set fish_term24bit 1`.

The `set_color` command uses the terminfo database to look up how to change terminal colors on whatever terminal is in use. Some systems have old and incomplete terminfo databases, and may lack color information for terminals that support it. Fish will assume that all terminals can use the [ANSI X3.64](https://en.wikipedia.org/wiki/ANSI_escape_code) escape sequences if the terminfo definition indicates a color below 16 is not supported.

Support for italics, dim, reverse, and other modes is not guaranteed in all terminal emulators. Fish attempts to determine if the terminal supports these modes even if the terminfo database may not be up-to-date.
