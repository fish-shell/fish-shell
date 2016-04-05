\section fish_right_prompt fish_right_prompt - define the appearance of the right-side command line prompt

\subsection fish_right_prompt-synopsis Synopsis
\fish{synopsis}
function fish_right_prompt
    ...
end
\endfish

\subsection fish_right_prompt-description Description

`fish_right_prompt` is similar to `fish_prompt`, except that it appears on the right side of the terminal window.

Multiple lines are not supported in `fish_right_prompt`.


\subsection fish_right_prompt-example Example

A simple right prompt:

\fish
function fish_right_prompt -d "Write out the right prompt"
    date '+%m/%d/%y'
end
\endfish

