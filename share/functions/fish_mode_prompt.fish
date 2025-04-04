# The fish_mode_prompt function is prepended to the prompt
function fish_mode_prompt --description "Displays the current mode"
    # Don't show mode indicator for final rendering in case transient mode is enabled
    if ! contains -- --final-rendering $argv
        # To reuse the mode indicator use this function instead
        fish_default_mode_prompt
    end
end
