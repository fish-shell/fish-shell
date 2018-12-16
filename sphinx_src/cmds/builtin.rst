\section builtin builtin - run a builtin command

\subsection builtin-synopsis Synopsis
\fish{synopsis}
builtin BUILTINNAME [OPTIONS...]
\endfish

\subsection builtin-description Description

`builtin` forces the shell to use a builtin command, rather than a function or program.

The following parameters are available:

- `-n` or `--names` List the names of all defined builtins


\subsection builtin-example Example

\fish
builtin jobs
# executes the jobs builtin, even if a function named jobs exists
\endfish
