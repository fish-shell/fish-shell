or - conditionally execute a command
==========================================


\subsection or-synopsis Synopsis
\fish{synopsis}
COMMAND1; or COMMAND2
\endfish

\subsection or-description Description

`or` is used to execute a command if the previous command was not successful (returned a status of something other than 0).

`or` statements may be used as part of the condition in an <a href="#if">`and`</a> or <a href="#while">`while`</a> block. See the documentation
for <a href="#if">`if`</a> and <a href="#while">`while`</a> for examples.

`or` does not change the current exit status itself, but the command it runs most likely will. The exit status of the last foreground command to exit can always be accessed using the <a href="index.html#variables-status">$status</a> variable.

\subsection or-example Example

The following code runs the `make` command to build a program. If the build succeeds, the program is installed. If either step fails, `make clean` is run, which removes the files created by the build process.

\fish
make; and make install; or make clean
\endfish
