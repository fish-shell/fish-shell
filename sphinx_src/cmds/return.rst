\section return return - stop the current inner function

\subsection return-synopsis Synopsis
\fish{synopsis}
function NAME; [COMMANDS...;] return [STATUS]; [COMMANDS...;] end
\endfish

\subsection return-description Description

`return` halts a currently running function. The exit status is set to `STATUS` if it is given.

It is usually added inside of a conditional block such as an <a href="#if">if</a> statement or a <a href="#switch">switch</a> statement to conditionally stop the executing function and return to the caller, but it can also be used to specify the exit status of a function.


\subsection return-example Example

The following code is an implementation of the false command as a fish function

\fish
function false
    return 1
end
\endfish


