# Define the default debugging prompt command.
function fish_breakpoint_prompt --description "A right prompt to be used when `breakpoint` is executed"
    set -l function (status current-function)
    set -l line (status current-line-number)
    # At the moment we don't include the filename because, even if we truncate it, it makes the
    # prompt too long.
    #set -l filename (status current-filename)
    #set -l prompt "$filename:$function:$line >"
    set -l prompt "$function:$line >"

    # Make sure the prompt doesn't consume more than half the terminal width.
    set -l max_len (math "$COLUMNS / 2")
    if test (string length -- $prompt) -gt $max_len
        set prompt ...(string sub -s -(math $max_len - 3) -- $prompt)
    end

    echo -ns (set_color $fish_color_status) "BP $prompt" (set_color normal) ' '
end
