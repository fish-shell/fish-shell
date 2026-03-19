# name: Simple Pythonista
# author: davbo

function fish_prompt
    if not set -q VIRTUAL_ENV_DISABLE_PROMPT
        set -g VIRTUAL_ENV_DISABLE_PROMPT true
    end
    set_color yellow
    printf '%s' $USER
    set_color --reset
    printf ' at '

    set_color magenta
    echo -n (prompt_hostname)
    set_color --reset
    printf ' in '

    set_color $fish_color_cwd
    printf '%s' (prompt_pwd)
    set_color --reset

    # Line 2
    echo
    if test -n "$VIRTUAL_ENV"
        printf "(%s) " (set_color blue)(path basename $VIRTUAL_ENV)(set_color --reset)
    end
    printf '↪ '
    set_color --reset
end
