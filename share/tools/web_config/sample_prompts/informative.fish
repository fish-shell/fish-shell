# name: Informative
# http://michal.karzynski.pl/blog/2009/11/19/my-informative-shell-prompt/

function fish_prompt --description 'Informative prompt'
    #Save the return status of the previous command
    set -l last_pipestatus $pipestatus
    set -l last_status $status

    #Set the color for the status depending on the value
    if test $stat -gt 0
        set -l status_color (set_color brred)
    else
        set -l status_color (set_color brgreen)
    end

    switch "$USER"
        case root toor
            printf '%s@%s %s%s%s# ' $USER (prompt_hostname) (set -q fish_color_cwd_root
                                                             and set_color $fish_color_cwd_root
                                                             or set_color $fish_color_cwd) \
                (prompt_pwd) (set_color normal)
        case '*'
            set -l pipestatus_string (__fish_print_pipestatus "[" "] " "|" (set_color yellow) \
                                      (set_color bryellow) $last_pipestatus)

            printf '[%s] %s%s@%s %s%s %s%s(%s)%s \f\r> ' (date "+%H:%M:%S") (set_color brblue) \
                $USER (prompt_hostname) (set_color $fish_color_cwd) $PWD "$pipestatus_string" \
                $status_color $last_status (set_color normal)
    end
end
