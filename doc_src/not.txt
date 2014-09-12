\section not not - negate the exit status of a job

\subsection not-synopsis Synopsis
\fish{synopsis}
not COMMAND [OPTIONS...]
\endfish

\subsection not-description Description

`not` negates the exit status of another command. If the exit status is zero, `not` returns 1. Otherwise, `not` returns 0.


\subsection not-example Example

The following code reports an error and exits if no file named spoon can be found.

\fish
if not test -f spoon
    echo There is no spoon
    exit 1
end
\endfish

