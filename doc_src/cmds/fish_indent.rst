.. _cmd-ghoti_indent:
.. program::ghoti_indent

ghoti_indent - indenter and prettifier
=====================================

Synopsis
--------

.. synopsis::

    ghoti_indent [OPTIONS] [FILE ...]


Description
-----------

:program:`ghoti_indent` is used to indent a piece of ghoti code. :program:`ghoti_indent` reads commands from standard input or the given filenames and outputs them to standard output or a specified file (if ``-w`` is given).

The following options are available:

**-w** or **--write**
    Indents a specified file and immediately writes to that file.

**-i** or **--no-indent**
    Do not indent commands; only reformat to one job per line.

**-c** or **--check**
    Do not indent, only return 0 if the code is already indented as ghoti_indent would, the number of failed files otherwise. Also print the failed filenames if not reading from standard input.

**-v** or **--version**
    Displays the current :program:`ghoti` version and then exits.

**--ansi**
    Colorizes the output using ANSI escape sequences, appropriate for the current :envvar:`TERM`, using the colors defined in the environment (such as :envvar:`ghoti_color_command`).

**--html**
    Outputs HTML, which supports syntax highlighting if the appropriate CSS is defined. The CSS class names are the same as the variable names, such as ``ghoti_color_command``.

**-d** or **--debug=DEBUG_CATEGORIES**
    Enable debug output and specify a pattern for matching debug categories. See :ref:`Debugging <debugging-ghoti>` in :doc:`ghoti <ghoti>` (1) for details.

**-o** or **--debug-output=DEBUG_FILE**
    Specify a file path to receive the debug output, including categories and ``ghoti_trace``. The default is standard error.

**--dump-parse-tree**
    Dumps information about the parsed statements to standard error. This is likely to be of interest only to people working on the ghoti source code.

**-h** or **--help**
    Displays help about using this command.
