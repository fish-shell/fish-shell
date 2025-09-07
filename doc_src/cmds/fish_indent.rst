.. _cmd-fish_indent:
.. program::fish_indent

fish_indent - indenter and prettifier
=====================================

Synopsis
--------

.. synopsis::

    fish_indent [OPTIONS] [FILE ...]


Description
-----------

:program:`fish_indent` is used to indent a piece of fish code. :program:`fish_indent` reads commands from standard input or the given filenames and outputs them to standard output or a specified file (if ``-w`` is given).

The following options are available:

**-w** or **--write**
    Indents a specified file and immediately writes to that file.

**-i** or **--no-indent**
    Do not indent commands; only reformat to one job per line.

**--only-indent**
    Do not reformat, only indent each line.

**--only-unindent**
    Do not reformat, only unindent each line.

**-c** or **--check**
    Do not indent, only return 0 if the code is already indented as fish_indent would, the number of failed files otherwise. Also print the failed filenames if not reading from standard input.

**-v** or **--version**
    Displays the current :program:`fish` version and then exits.

**--ansi**
    Colorizes the output using ANSI escape sequences using the colors defined in the environment (such as :envvar:`fish_color_command`).

**--html**
    Outputs HTML, which supports syntax highlighting if the appropriate CSS is defined. The CSS class names are the same as the variable names, such as ``fish_color_command``.

**--dump-parse-tree**
    Dumps information about the parsed statements to standard error. This is likely to be of interest only to people working on the fish source code.

**-h** or **--help**
    Displays help about using this command.
