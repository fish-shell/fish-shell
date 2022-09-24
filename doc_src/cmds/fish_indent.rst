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

**-c** or **--check**
    Do not indent, only return 0 if the code is already indented as fish_indent would, the number of failed files otherwise. Also print the failed filenames if not reading from standard input.

**-v** or **--version**
    Displays the current :program:`fish` version and then exits.

**--ansi**
    Colorizes the output using ANSI escape sequences, appropriate for the current :envvar:`TERM`, using the colors defined in the environment (such as :envvar:`fish_color_command`).

**--html**
    Outputs HTML, which supports syntax highlighting if the appropriate CSS is defined. The CSS class names are the same as the variable names, such as ``fish_color_command``.

**-d** or **--debug=DEBUG_CATEGORIES**
    Enable debug output and specify a pattern for matching debug categories. See :ref:`Debugging <debugging-fish>` in :doc:`fish <fish>` (1) for details.

**-o** or **--debug-output=DEBUG_FILE**
    Specify a file path to receive the debug output, including categories and ``fish_trace``. The default is standard error.

**--dump-parse-tree**
    Dumps information about the parsed statements to standard error. This is likely to be of interest only to people working on the fish source code.

**-h** or **--help**
    Displays help about using this command.
