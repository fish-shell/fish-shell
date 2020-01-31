.. _cmd-fish_breakpoint_prompt:

fish_breakpoint_prompt - define the prompt when stopped at a breakpoint
=======================================================================

Synopsis
--------

::

    function fish_breakpoint_prompt
        ...
    end


Description
-----------

By defining the ``fish_breakpoint_prompt`` function, the user can choose a custom prompt when asking for input in response to a :ref:`breakpoint <cmd-breakpoint>` command. The ``fish_breakpoint_prompt`` function is executed when the prompt is to be shown, and the output is used as a prompt.

The exit status of commands within ``fish_breakpoint_prompt`` will not modify the value of :ref:`$status <variables-status>` outside of the ``fish_breakpoint_prompt`` function.

``fish`` ships with a default version of this function that displays the function name and line number of the current execution context.


Example
-------

A simple prompt that is a simplified version of the default debugging prompt::

    function fish_breakpoint_prompt -d "Write out the debug prompt"
        set -l function (status current-function)
        set -l line (status current-line-number)
        set -l prompt "$function:$line >"
        echo -ns (set_color $fish_color_status) "BP $prompt" (set_color normal) ' '
    end

