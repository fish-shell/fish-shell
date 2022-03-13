.. _cmd-function:

function - create a function
============================

Synopsis
--------

.. synopsis::

    function NAME [OPTIONS]; BODY; end


Description
-----------

``function`` creates a new function *NAME* with the body *BODY*.

A function is a list of commands that will be executed when the name of the function is given as a command.

The following options are available:

**-a** *NAMES* or **--argument-names** *NAMES*
    Assigns the value of successive command-line arguments to the names given in *NAMES*.

**-d** *DESCRIPTION* or **--description** *DESCRIPTION*
    A description of what the function does, suitable as a completion description.

**-w** *WRAPPED_COMMAND* or **--wraps** *WRAPPED_COMMAND*
    Inherit completions from the given *WRAPPED_COMMAND*. See the documentation for :ref:`complete <cmd-complete>` for more information.

**-e** *EVENT_NAME* or **--on-event** *EVENT_NAME*
    Run this function when the specified named event is emitted. Fish internally generates named events, for example,when showing the prompt. Custom events can be emitted using the :ref:`emit <cmd-emit>` command.

**-v** *VARIABLE_NAME* or **--on-variable** *VARIABLE_NAME*
    Run this function when the variable *VARIABLE_NAME* changes value. Note that :program:`fish`` makes no guarantees on any particular timing or even that the function will be run for every single ``set``. Rather it will be run when the variable has been set at least once, possibly skipping some values or being run when the variable has been set to the same value (except for universal variables set in other shells - only changes in the value will be picked up for those).

**-j** *PID* or **--on-job-exit** *PID*
    Run this function when the job containing a child process with the given process identifier *PID* exits. Instead of a PID, the string 'caller' can be specified. This is only allowed when in a command substitution, and will result in the handler being triggered by the exit of the job which created this command substitution.

**-p** *PID* or **--on-process-exit** *PID*
    Run this function when the fish child process with process ID PID exits. Instead of a PID, for backward compatibility, "``%self``" can be specified as an alias for ``$fish_pid``, and the function will be run when the current fish instance exits.

**-s** *SIGSPEC* or **--on-signal** *SIGSPEC*
    Run this function when the signal ``SIGSPEC`` is delivered. ``SIGSPEC`` can be a signal number, or the signal name, such as ``SIGHUP`` (or just ``HUP``). Note that the signal must have been delivered to :program:`fish`; for example, :kbd:`Ctrl-C` sends ``SIGINT`` to the foreground process group, which will not be :program:`fish` if you are running another command at the time.

**-S** or **--no-scope-shadowing**
    Allows the function to access the variables of calling functions. Normally, any variables inside the function that have the same name as variables from the calling function are "shadowed", and their contents are independent of the calling function.

    It's important to note that this does not capture referenced variables or the scope at the time of function declaration! At this time, fish does not have any concept of closures, and variable lifetimes are never extended. In other words, by using **--no-scope-shadowing** the scope of the function each time it is run is shared with the scope it was *called* from rather than the scope it was *defined* in.

**-V** or **--inherit-variable NAME**
    Snapshots the value of the variable ``NAME`` and defines a local variable with that same name and value when the function is defined. This is similar to a closure in other languages like Python but a bit different. Note the word "snapshot" in the first sentence. If you change the value of the variable after defining the function, even if you do so in the same scope (typically another function) the new value will not be used by the function you just created using this option. See the ``function notify`` example below for how this might be used.

If the user enters any additional arguments after the function, they are inserted into the environment :ref:`variable list <variables-lists>` ``$argv``. If the ``--argument-names`` option is provided, the arguments are also assigned to names specified in that option.

By using one of the event handler switches, a function can be made to run automatically at specific events. The user may generate new events using the :ref:`emit <cmd-emit>` builtin. Fish generates the following named events:

- ``fish_prompt``, which is emitted whenever a new fish prompt is about to be displayed.

- ``fish_preexec``, which is emitted right before executing an interactive command. The commandline is passed as the first parameter. Not emitted if command is empty.

- ``fish_posterror``, which is emitted right after executing a command with syntax errors. The commandline is passed as the first parameter.

- ``fish_postexec``, which is emitted right after executing an interactive command. The commandline is passed as the first parameter. Not emitted if command is empty.

- ``fish_exit`` is emitted right before fish exits.

- ``fish_cancel``, which is emitted when a commandline is cleared (used for terminal-shell integration).

Example
-------



::

    function ll
        ls -l $argv
    end


will run the ``ls`` command, using the ``-l`` option, while passing on any additional files and switches to ``ls``.



::

    function mkdir -d "Create a directory and set CWD"
        command mkdir $argv
        if test $status = 0
            switch $argv[(count $argv)]
                case '-*'
    
                case '*'
                    cd $argv[(count $argv)]
                    return
            end
        end
    end


This will run the ``mkdir`` command, and if it is successful, change the current working directory to the one just created.



::

    function notify
        set -l job (jobs -l -g)
        or begin; echo "There are no jobs" >&2; return 1; end
    
        function _notify_job_$job --on-job-exit $job --inherit-variable job
            echo -n \a # beep
            functions -e _notify_job_$job
        end
    end


This will beep when the most recent job completes.


Notes
-----

Events are only received from the current fish process as there is no way to send events from one fish process to another.

See more
--------

For more explanation of how functions fit into fish, see :ref:`Functions <syntax-function>`.
