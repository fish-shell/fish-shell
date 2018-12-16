\section fish_prompt fish_prompt - define the appearance of the command line prompt

\subsection fish_prompt-synopsis Synopsis
\fish{synopsis}
function fish_prompt
    ...
end
\endfish

\subsection fish_prompt-description Description

By defining the `fish_prompt` function, the user can choose a custom prompt. The `fish_prompt` function is executed when the prompt is to be shown, and the output is used as a prompt.

The exit status of commands within `fish_prompt` will not modify the value of <a href="index.html#variables-status">$status</a> outside of the `fish_prompt` function.

`fish` ships with a number of example prompts that can be chosen with the `fish_config` command.


\subsection fish_prompt-example Example

A simple prompt:

\fish
function fish_prompt -d "Write out the prompt"
    printf '%s@%s%s%s%s> ' (whoami) (hostname | cut -d . -f 1) \
    		(set_color $fish_color_cwd) (prompt_pwd) (set_color normal)
end
\endfish

