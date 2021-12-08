function fish_title --argument-names command_override --desription 'Sets fish title'
    # emacs' "term" is basically the only term that can't handle it.
    if not set --query INSIDE_EMACS; or string match --quiet --invert '*,term:*' -- $INSIDE_EMACS
        # An override for the current command is passed as the first parameter.
        # This is used by `fg` to show the true process name, among others.
        echo (set --query $command_override
            and echo $command_override
            or status current-command) (__fish_pwd)
    end
end
