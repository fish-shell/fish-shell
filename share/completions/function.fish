function __fish_complete_signals
    __fish_make_completion_signals
    for sig in $__kill_signals
        # signal name as completion and signal number as description
        printf "%s\t%s\n" (string split " " $sig)[-1..1]
    end
end

function __fish_complete_variables
    # Borrowed from completions for set command
    set -l | string replace ' ' \t'Local Variable: '
    set -g | string replace ' ' \t'Global Variable: '
    set -U | string replace ' ' \t'Universal Variable: '
end

function __fish_complete_function_event_handlers
    set -l handlers fish_prompt "When new prompt is about to be displayed" \
        fish_command_not_found "When command lookup fails" \
        fish_preexec "Before executing an interactive command" \
        fish_postexec "After executing an interactive command" \
        fish_exit "Right before fish exits" \
        fish_cancel "When commandline is cleared" \
        fish_posterror "After executing command with syntax errors" \
        fish_focus_in "When fish's terminal gains focus" \
        fish_focus_out "When fish's terminal loses focus"

    printf "%s\t%s\n" $handlers
end

complete -c function -s d -l description -d "Set function description" -x
complete -c function -xa "(functions -n)" -d Function
complete -c function -xa "(builtin -n)" -d Builtin
complete -c function -s j -l on-job-exit -d "Make the function a job exit event handler" -x
complete -c function -s p -l on-process-exit -d "Make the function a process exit event handler" -x
complete -c function -s s -l on-signal -d "Make the function a signal event handler" -xka "(__fish_complete_signals)"
complete -c function -s v -l on-variable -d "Make the function a variable update event handler" -xa "(__fish_complete_variables)"
complete -c function -s e -l on-event -d "Make the function a generic event handler" -xa "(__fish_complete_function_event_handlers)"
complete -c function -s a -l argument-names -d "Specify named arguments" -x
complete -c function -s S -l no-scope-shadowing -d "Do not shadow variable scope of calling function"
complete -c function -s w -l wraps -d "Inherit completions from the given command" -xa "(__fish_complete_command)"
complete -c function -s V -l inherit-variable -d "Snapshot and define local variable" -xa "(__fish_complete_variables)"
