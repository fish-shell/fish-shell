# name: Informative
# http://michal.karzynski.pl/blog/2009/11/19/my-informative-shell-prompt/

function ghoti_prompt --description 'Informative prompt'
    #Save the return status of the previous command
    set -l last_pipestatus $pipestatus
    set -lx __ghoti_last_status $status # Export for __ghoti_print_pipestatus.

    if functions -q ghoti_is_root_user; and ghoti_is_root_user
        printf '%s@%s %s%s%s# ' $USER (prompt_hostname) (set -q ghoti_color_cwd_root
                                                         and set_color $ghoti_color_cwd_root
                                                         or set_color $ghoti_color_cwd) \
            (prompt_pwd) (set_color normal)
    else
        set -l status_color (set_color $ghoti_color_status)
        set -l statusb_color (set_color --bold $ghoti_color_status)
        set -l pipestatus_string (__ghoti_print_pipestatus "[" "]" "|" "$status_color" "$statusb_color" $last_pipestatus)

        printf '[%s] %s%s@%s %s%s %s%s%s \n> ' (date "+%H:%M:%S") (set_color brblue) \
            $USER (prompt_hostname) (set_color $ghoti_color_cwd) $PWD $pipestatus_string \
            (set_color normal)
    end
end
