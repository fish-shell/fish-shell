\section exit exit - exit the shell

\subsection exit-synopsis Synopsis
\fish{synopsis}
exit [STATUS]
\endfish

\subsection exit-description Description

`exit` causes fish to exit. If `STATUS` is supplied, it will be converted to an integer and used as the exit code. Otherwise, the exit code will be that of the last command executed.

If exit is called while sourcing a file (using the <a href="#source">source</a> builtin) the rest of the file will be skipped, but the shell itself will not exit.
