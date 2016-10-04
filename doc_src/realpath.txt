\section realpath realpath - Convert a path to an absolute path without symlinks

\subsection realpath-synopsis Synopsis
\fish{synopsis}
realpath path
\endfish

\subsection realpath-description Description

This is implemented as a function and a builtin. The function will attempt to use an external realpath command if one can be found. Otherwise it falls back to the builtin.  The builtin does not support any options. It's meant to be used only by scripts which need to be portable. The builtin implementation behaves like GNU realpath when invoked without any options (which is the most common use case). In general scripts should not invoke the builtin directly. They should just use `realpath`.

If the path is invalid no translated path will be written to stdout and an error will be reported.
