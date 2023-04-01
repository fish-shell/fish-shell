function __ghoti_complete_signals
    __ghoti_make_completion_signals
    for sig in $__kill_signals
        # signal name as completion and signal number as description
        printf "%s\t%s\n" (string split " " $sig)[-1..1]
    end
end

function __ghoti_complete_variables
    # Borrowed from completions for set command
    set -l | string replace ' ' \t'Local Variable: '
    set -g | string replace ' ' \t'Global Variable: '
    set -U | string replace ' ' \t'Universal Variable: '
end

function __ghoti_complete_function_event_handlers
    set -l handlers ghoti_prompt "When new prompt is about to be displayed" \
        ghoti_command_not_found "When command lookup fails" \
        ghoti_preexec "Before executing an interactive command" \
        ghoti_postexec "After executing an interactive command" \
        ghoti_exit "Right before ghoti exits" \
        ghoti_cancel "When commandline is cleared" \
        ghoti_posterror "After executing command with syntax errors"

    printf "%s\t%s\n" $handlers
end

complete -c function -s d -l description -d "Set function description" -x
complete -c function -xa "(functions -n)" -d Function
complete -c function -xa "(builtin -n)" -d Builtin
complete -c function -s j -l on-job-exit -d "Make the function a job exit event handler" -x
complete -c function -s p -l on-process-exit -d "Make the function a process exit event handler" -x
complete -c function -s s -l on-signal -d "Make the function a signal event handler" -xka "(__ghoti_complete_signals)"
complete -c function -s v -l on-variable -d "Make the function a variable update event handler" -xa "(__ghoti_complete_variables)"
complete -c function -s e -l on-event -d "Make the function a generic event handler" -xa "(__ghoti_complete_function_event_handlers)"
complete -c function -s a -l argument-names -d "Specify named arguments" -x
complete -c function -s S -l no-scope-shadowing -d "Do not shadow variable scope of calling function"
complete -c function -s w -l wraps -d "Inherit completions from the given command" -xa "(__ghoti_complete_command)"
complete -c function -s V -l inherit-variable -d "Snapshot and define local variable" -xa "(__ghoti_complete_variables)"
