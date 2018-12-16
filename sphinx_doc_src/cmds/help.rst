\section help help - display fish documentation

\subsection help-synopsis Synopsis
\fish{synopsis}
help [SECTION]
\endfish

\subsection help-description Description

`help` displays the fish help documentation.

If a `SECTION` is specified, the help for that command is shown.

If the BROWSER environment variable is set, it will be used to display the documentation. Otherwise, fish will search for a suitable browser.

If you prefer to use a different browser (other than as described above) for fish help, you can set the fish_help_browser variable. This variable may be set as an array, where the first element is the browser command and the rest are browser options.

Note that most builtin commands display their help in the terminal when given the `--help` option.


\subsection help-example Example

`help fg` shows the documentation for the `fg` builtin.
