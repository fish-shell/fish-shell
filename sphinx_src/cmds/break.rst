\section break break - stop the current inner loop

\subsection break-synopsis Synopsis
\fish{synopsis}
LOOP_CONSTRUCT; [COMMANDS...] break; [COMMANDS...] end
\endfish

\subsection break-description Description

`break` halts a currently running loop, such as a <a href="#for">for</a> loop or a <a href="#while">while</a> loop. It is usually added inside of a conditional block such as an <a href="#if">if</a> statement or a <a href="#switch">switch</a> statement.

There are no parameters for `break`.


\subsection break-example Example
The following code searches all .c files for "smurf", and halts at the first occurrence.

\fish
for i in *.c
    if grep smurf $i
        echo Smurfs are present in $i
        break
    end
end
\endfish
