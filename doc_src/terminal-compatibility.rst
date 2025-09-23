Terminal Compatibility
======================

fish writes various control sequences to the terminal.
Some must be implemented to enable basic functionality,
while others enable optional features and may be ignored by the terminal.

The terminal must be able to parse Control Sequence Introducer (CSI) commands, Operating System Commands (OSC) and optionally Device Control Strings (DCS).
These are defined by ECMA-48.
If a valid CSI, OSC or DCS sequence does not represent a command implemented by the terminal, the terminal must ignore it.

Control sequences are denoted in a fish-like syntax.
Special characters other than ``\`` are not escaped.
Spaces are only added for readability and are not part of the sequence.
Placeholders are written as ``<Ps>`` for a number or ``<Pt>`` for an arbitrary printable string.

**NOTE:** fish does not rely on your system's terminfo database.
In this document, terminfo (TI) codes are included for reference only.

Required Commands
-----------------

.. list-table::
   :widths: auto
   :header-rows: 1

   * - Sequence
     - TI
     - Description
     - Origin
   * - ``\r``
     - n/a
     - Move cursor to the beginning of the line
     -
   * - ``\n``
     - cud1
     - Move cursor down one line.
     -
   * - ``\e[ Ps A``
     - cuu
     - Move cursor up Ps columns, or one column if no parameter.
     - VT100
   * - ``\e[ Ps C``
     - cuf
     - Move cursor to the right Ps columns, or one column if no parameter.
     - VT100
   * - ``\x08``
     - cub1
     - Move cursor one column to the left.
     - VT100
   * - ``\e[ Ps D``
     - cub
     - Move cursor to the left Ps times.
     - VT100
   * - ``\e[H``
     - cup
     - Set cursor position (no parameters means: move to row 1, column 1).
     - VT100
   * - ``\e[K``
     - el
     - Clear to end of line.
     - VT100
   * - ``\e[J``
     - ed
     - Clear to the end of screen.
     - VT100
   * - ``\e[2J``
     - clear
     - Clear the screen.
     - VT100
   * - ``\e[0c``
     -
     - Request primary device attribute.
       The terminal must respond with a CSI command that starts with the ``?`` parameter byte (so a sequence starting with ``\e[?``) and has ``c`` as final byte.
     - VT100
   * - n/a
     - am
     - Soft wrap text at screen width.
     -
   * - n/a
     - xenl
     - Printing to the last column does not move the cursor to the next line.
       Verify this by running ``printf %0"$COLUMNS"d 0; sleep 3``
     -

Optional Commands
-----------------

.. list-table::
   :widths: auto
   :header-rows: 1

   * - Sequence
     - TI
     - Description
     - Origin
   * - ``\t``
     - it
     - Move the cursor to the next tab stop (à 8 columns).
       This is mainly relevant if your prompt includes tabs.
     -

   * - ``\e[m``
     - sgr0
     - Turn off bold/dim/italic/underline/reverse attribute modes and select default colors.
     -
   * - ``\e[1m``
     - bold
     - Enter bold mode.
     -
   * - ``\e[2m``
     - dim
     - Enter dim mode.
     -
   * - ``\e[3m``
     - sitm
     - Enter italic mode.
     -
   * - ``\e[4m``
     - smul
     - Enter underline mode.
     -
   * - ``\e[4:2m``
     - Su
     - Enter double underline mode.
     - kitty
   * - ``\e[4:3m``
     - Su
     - Enter curly underline mode.
     - kitty
   * - ``\e[4:4m``
     - Su
     - Enter dotted underline mode.
     - kitty
   * - ``\e[4:5m``
     - Su
     - Enter dashed underline mode.
     - kitty
   * - ``\e[7m``
     - rev
     - Enter reverse video mode (swap foreground and background colors).
     -
   * - ``\e[23m``
     - ritm
     - Exit italic mode.
     -
   * - ``\e[24m``
     - rmul
     - Exit underline mode.
     -
   * - ``\e[38;5; Ps m``
     - setaf
     - Select foreground color Ps from the 256-color-palette.
     -
   * - ``\e[48;5; Ps m``
     - setab
     - Select background color Ps from the 256-color-palette.
     -
   * - ``\e[58:5: Ps m`` (note: colons not semicolons)
     - Su
     - Select underline color Ps from the 256-color-palette.
     - kitty
   * - ``\e[ Ps m``
     - setaf
       setab
     - Select foreground/background color. This uses a color in the aforementioned 256-color-palette, based on the range that contains the parameter:
       30-37 maps to foreground 0-7,
       40-47 maps to background 0-7,
       90-97 maps to foreground 8-15 and
       100-107 maps to background 8-15.
     -
   * - ``\e[38;2; Ps ; Ps ; Ps m``
     -
     - Select foreground color from 24-bit RGB colors.
     -
   * - ``\e[48;2; Ps ; Ps ; Ps m``
     -
     - Select background color from 24-bit RGB colors.
     -
   * - ``\e[49m``
     -
     - Reset background color to the terminal's default.
     -
   * - ``\e[58:2:: Ps : Ps : Ps m`` (note: colons not semicolons)
     - Su
     - Select underline color from 24-bit RGB colors.
     - kitty
   * - ``\e[59m``
     - Su
     - Reset underline color to the default (follow the foreground color).
     - kitty
   * - .. _indn:

       ``\e[ Ps S``
     - indn
     - Scroll up Ps lines (aka ``SU`` but terminfo calls it "scroll forward")
       This enables the :ref:`scrollback-push <special-input-functions-scrollback-push>` special input function which is used by :kbd:`ctrl-l`.
     - ECMA-48
   * - ``\e[= Ps u``, ``\e[? Ps u``
     - n/a
     - Enable the kitty keyboard protocol.
     - kitty
   * - .. _cursor-position-report:

       ``\e[6n``
     - n/a
     - Request cursor position report.
       The response must be of the form ``\e[ Ps ; Ps R``
       where the first parameter is the row number
       and the second parameter is the column number.
       Both start at 1.

       This is used inside terminals that either

       - implement the OSC 133 `click_events <#osc-133>`_ feature.
       - advertise the `indn`_ capability via `XTGETTCAP`_
     - VT100
   * - ``\e[ \x20 q``
     - Se
     - Reset cursor style to the terminal's default. This is not used as of today but may be
       in future.
     - VT520
   * - ``\e[ Ps \x20 q``
     - Ss
     - Set cursor style (DECSCUSR); Ps is 2, 4 or 6 for block, underscore or line shape.
     - VT520
   * - ``\e[ Ps q``
     - n/a
     - Request terminal name and version (XTVERSION).
     - XTerm
   * - ``\e[?25h``
     - cvvis
     - Enable cursor visibility (DECTCEM).
     - VT220
   * - ``\e[?1000l``
     - n/a
     - Disable mouse reporting.
     - XTerm
   * - ``\e[?1004h``
     - n/a
     - Enable focus reporting.
     -
   * - ``\e[?1004l``
     - n/a
     - Disable focus reporting.
     -
   * - ``\e[?1049h``
     - n/a
     - Enable alternate screen buffer.
     - XTerm
   * - ``\e[?1049l``
     - n/a
     - Disable alternate screen buffer.
     - XTerm
   * - ``\e[?2004h``
     -
     - Enable bracketed paste.
     - XTerm
   * - ``\e[?2004l``
     -
     - Disable bracketed paste.
     - XTerm
   * - ``\e]0; Pt \x07``
     - ts
     - Set window title (OSC 0).
     - XTerm
   * - ``\e]7;file:// Pt / Pt \x07``
     -
     - Report working directory (OSC 7).
       Since the terminal may be running on a different system than a (remote) shell,
       the hostname (first parameter) will *not* be ``localhost``.
     - iTerm2
   * - ``\e]8;; Pt \e\\``
     -
     - Create a `hyperlink (OSC 8) <https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda>`_.
       This is used in fish's man pages.
     - GNOME Terminal
   * - ``\e]52;c; Pt \x07``
     -
     - Copy to clipboard (OSC 52).
     - XTerm
   * - .. _osc-133:

       ``\e]133;A; click_events=1\x07``
     -
     - Mark prompt start (OSC 133), with kitty's ``click_events`` extension.
       If ``click_events`` is implemented,
       the `cursor position reporting <#cursor-position-report>`_ feature is required.
     - FinalTerm, kitty
   * - ``\e]133;C; cmdline_url= Pt \x07``
     -
     - Mark command start (OSC 133), with kitty's ``cmdline_url`` extension whose parameter is the URL-encoded command line.
     - FinalTerm, kitty
   * - ``\e]133;D; Ps \x07``
     -
     - Mark command end (OSC 133);  Ps is the exit status.
     - FinalTerm
   * - .. _xtgettcap:

        ``\eP+q Pt \e\\``
     -
     - Request terminfo capability (XTGETTCAP).
       The parameter is the capability's hex-encoded terminfo code.
       To advertise a capability, the response must of the form
       ``\eP1+q Pt \e\\`` or ``\eP1+q Pt = Pt \e\\``.
       In either variant the first parameter must be the hex-encoded terminfo code.
       The second variant's second parameter is ignored.

       Currently, fish only queries the `indn`_ string capability.
     - XTerm (but without string capabilities), kitty;
       also adopted by foot, wezterm, contour, ghostty
