\section else else - execute command if a condition is not met

\subsection else-synopsis Synopsis
\fish{synopsis}
if CONDITION; COMMANDS_TRUE...; [else; COMMANDS_FALSE...;] end
\endfish

\subsection else-description Description

`if` will execute the command `CONDITION`. If the condition's exit status is 0, the commands `COMMANDS_TRUE` will execute. If it is not 0 and `else` is given, `COMMANDS_FALSE` will be executed.


\subsection else-example Example

The following code tests whether a file `foo.txt` exists as a regular file.

\fish
if test -f foo.txt
    echo foo.txt exists
else
    echo foo.txt does not exist
end
\endfish
