.. _cmd-ghoti_breakpoint_prompt:

ghoti_breakpoint_prompt - define the prompt when stopped at a breakpoint
=======================================================================

Synopsis
--------

.. synopsis::

    ghoti_breakpoint_prompt

::

    function ghoti_breakpoint_prompt
        ...
    end


Description
-----------

``ghoti_breakpoint_prompt`` is the prompt function when asking for input in response to a :doc:`breakpoint <breakpoint>` command.

The exit status of commands within ``ghoti_breakpoint_prompt`` will not modify the value of :ref:`$status <variables-status>` outside of the ``ghoti_breakpoint_prompt`` function.

``ghoti`` ships with a default version of this function that displays the function name and line number of the current execution context.


Example
-------

A simple prompt that is a simplified version of the default debugging prompt::

    function ghoti_breakpoint_prompt -d "Write out the debug prompt"
        set -l function (status current-function)
        set -l line (status current-line-number)
        set -l prompt "$function:$line >"
        echo -ns (set_color $ghoti_color_status) "BP $prompt" (set_color normal) ' '
    end

