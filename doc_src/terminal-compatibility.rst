Terminal Compatibility
======================

fish requires the terminal to handle various control sequences.
fish does *not* use the Terminfo database indicated by :envvar:`TERM`.

Fish requires certain terminal features to be present and won't work correctly without. In addition
it tries to enable optional features and needs those sequences to either be handled or ignored.

To make full use of your terminal, fish emits a number of control sequences and control characters.

The terminal must be able to parse Control Sequence Introducer (CSI) commands, Operating System Commands (OSC) and optionally Device Control Strings (DCS).
These are defined by ECMA-48.
If a valid CSI, OSC or DCS sequence does not represent a command implemented by the terminal, the terminal should ignore it.

Here are the commands used by fish.

Control sequences are denoted in a fish-like syntax:
special characters like ``[`` and ``;`` are not escaped (except for backslash); spaces are only added for readability.
Placeholders are written in angle brackets: ``<Ps>`` for a number and ``<Pt>`` for arbitrary printable strings.

Required Commands
-----------------

.. list-table::
   :widths: auto
   :header-rows: 1

   * - Sequence
     - Description
     - Origin
   * - ``\n``
     - move cursor down one line (Terminfo: ``cursor_down`` / ``cud1``)
     -
   * - ``\e[ <Ps> A``
     - move cursor up <Ps> times (Terminfo: ``parm_up_cursor`` / ``cuu``)
     - VT100
   * - ``\e[ <Ps> C``
     - move cursor to the right <Ps> times (Terminfo: ``parm_right_cursor`` / ``cuf``)
     - VT100
   * - ``\e[ <Ps> D``
     - move cursor to the left <Ps> times (Terminfo: ``parm_left_cursor`` / ``cub``)
     - VT100
   * - ``\e[H``
     - set cursor position (move to row 1, column 1) (Terminfo: ``cursor_address`` / ``cup``)
     - VT100
   * - ``\e[K``
     - clear to end of line (Terminfo: ``clr_eol`` / ``el``)
     - VT100
   * - ``\e[J``
     - clear to the end of screen (Terminfo: ``clr_eos`` / ``ed``)
     - VT100
   * - ``\e[2J``
     - clear the entire screen (Terminfo: ``clear_screen`` / ``clear``)
     - VT100
   * - ``\e[0c``
     - request primary device attribute. The terminal must respond. An example response is ``\e[?62;4;22;28c``.
     - VT100
   * - n/a
     - soft wrapping (Terminfo: ``auto_right_margin`` / ``am``)
     -

Optional Commands
-----------------

.. list-table::
   :widths: auto
   :header-rows: 1

   * - Sequence
     - Description
     - Origin
   * - ``\t``
     - move the cursor to the next tab stop (à 8 colums).
       This is mainly relevant if your prompt includes tabs.
     -

   * - ``\e[m``
     - turn off bold/dim/italic/underline/reverse attributes (Terminfo: ``exit_attribute_mode`` / ``sgr0``)
     -
   * - ``\e[1m``
     - enter bold mode (Terminfo: ``enter_bold_mode`` / ``bold``)
     -
   * - ``\e[2m``
     - enter dim mode (Terminfo: ``enter_dim_mode`` / ``dim``)
     -
   * - ``\e[3m``
     - enter italic mode (Terminfo: ``enter_italics_mode`` / ``sitm``)
     -
   * - ``\e[4m``
     - enter underline mode (Terminfo: ``enter_underline_mode`` / ``smul``)
     -
   * - ``\e[7m``
     - enter reverse mode (Terminfo: ``enter_reverse_mode`` / ``rev``)
     -
   * - ``\e[23m``
     - exit italic mode (Terminfo: ``exit_italics_mode`` / ``ritm``)
     -
   * - ``\e[24m``
     - exit underline mode (Terminfo: ``exit_underline_mode`` / ``rmul``)
     -
   * - ``\e[38;5; <Ps> m``
     - select foreground color <Ps> from the 256-color-palette
     -
   * - ``\e[48;5; <Ps> m``
     - select background color <Ps> from the 256-color-palette
     -
   * - ``\e[ <Ps> m``
     - these map to colors in the above 256-color-palette, depending on the range that contains <Ps>:
       30-37 maps to foreground 0-7,
       40-47 maps to background 0-7,
       90-97 maps to foreground 8-15,
       100-107 maps to background 8-15
     -
   * - ``\e[38;2;<Ps>;<Ps>;<Ps>m``
     - select foreground color from 24-bit RGB colors
     -
   * - ``\e[48;2;<Ps>;<Ps>;<Ps>m``
     - select background color from 24-bit RGB colors
     -
   * - ``\e[ <Ps> S``
     - scroll forward <Ps> lines (Terminfo: ``parm_index`` / ``indn``)
     -
   * - ``\e]0; <Pt> \x07``
     - OSC 0 window title
     - XTerm
   * - ``\e]7;file:// <Pt> / <Pt> \x07``
     - OSC 7 working directory reporting.
       To support remote shells, the hostname (first parameter) will *not* be ``localhost``
     - iTerm2
   * - ``\e]52;c; <Pt> \x07``
     - OSC 52 copy to clipboard
     - XTerm
   * - ``\e]133;A;click_events=1\x07``
     - OSC 133 prompt start, with kitty's ``click_events`` extension
     - FinalTerm/kitty
   * - ``\e]133;C;cmdline_url= <Pt> \x07``
     - OSC 133 command start, with kitty's ``cmdline_url`` extension whose parameter is the URL-encoded command line.
     - FinalTerm/kitty
   * - ``\e]133;D; <Ps> \x07``
     - OSC 133 command finished; <Ps> is the exit status
     - FinalTerm
   * - ``\e[= <Ps> u``, ``\e[? <Ps> u``
     - kitty keyboard protocol
     - kitty
   * - ``\e[?2004h <Pt> \e[?2004l``
     - bracketed paste
     - XTerm
   * - ``\e[?2004h``
     - enable bracketed paste
     - XTerm
   * - ``\e[?2004l``
     - disable bracketed paste
     - XTerm
   * - ``\e[?1049h``
     - enable alternate screen buffer
     - XTerm
   * - ``\e[?1049l``
     - disable alternate screen buffer
     - XTerm
   * - ``\e[6n``
     - request cursor position cursor position reporting
     - VT100
   * - ``\e[?1000l``
     - disable mouse reporting
     - XTerm
   * - ``\eP+q <Pt> \e\\``
     - DCS + q; -- query the terminal's builtin Terminfo database (XTGETTCAP), where <Pt> is the hex-encoded Terminfo name.
       fish asks for the ``indn`` capability (note that string capabilities are only supported by kitty and foot)
     - XTerm/kitty/foot
   * - ``\e> <Ps> q``
     - query terminal name and version (XTVERSION)
     - XTerm
   * - ``\e[?2026h``
     - begin synchronized output
     - contour
   * - ``\e[?2026l``
     - end synchronized output
     - contour
   * - ``\e[?25h``
     - cursor visibililty (DECTCEM)
     - VT220
   * - ``\e[?1004h``
     - enable focus reporting
     -
   * - ``\e[?1004l``
     - disable focus reporting
     -
