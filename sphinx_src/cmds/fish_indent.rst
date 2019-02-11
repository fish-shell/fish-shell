\section fish_indent fish_indent - indenter and prettifier

\subsection fish_indent-synopsis Synopsis
\fish{synopsis}
fish_indent [OPTIONS]
\endfish

\subsection fish_indent-description Description

`fish_indent` is used to indent a piece of fish code. `fish_indent` reads commands from standard input and outputs them to standard output or a specified file.

The following options are available:

- `-w` or `--write` indents a specified file and immediately writes to that file.

- `-i` or `--no-indent` do not indent commands; only reformat to one job per line.

- `-v` or `--version` displays the current fish version and then exits.

- `--ansi` colorizes the output using ANSI escape sequences, appropriate for the current $TERM, using the colors defined in the environment (such as `$fish_color_command`).

- `--html` outputs HTML, which supports syntax highlighting if the appropriate CSS is defined. The CSS class names are the same as the variable names, such as `fish_color_command`.

- `-d` or `--debug-level=DEBUG_LEVEL` enables debug output and specifies a verbosity level (like `fish -d`). Defaults to 0.

- `-D` or `--debug-stack-frames=DEBUG_LEVEL` specify how many stack frames to display when debug messages are written. The default is zero. A value of 3 or 4 is usually sufficient to gain insight into how a given debug call was reached but you can specify a value up to 128.

- `--dump-parse-tree` dumps information about the parsed statements to stderr. This is likely to be of interest only to people working on the fish source code.
