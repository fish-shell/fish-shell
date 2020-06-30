# name: Informative
# http://michal.karzynski.pl/blog/2009/11/19/my-informative-shell-prompt/

function fish_prompt --description 'Informative prompt'
    #Save the return status of the previous command
    set -l last_pipestatus $pipestatus
    set -l last_status $status

    if fish_is_root_user
        printf '%s@%s %s%s%s# ' $USER (prompt_hostname) (set -q fish_color_cwd_root
                                                         and set_color $fish_color_cwd_root
                                                         or set_color $fish_color_cwd) \
            (prompt_pwd) (set_color normal)
    else
        set -l pipestatus_string (__fish_print_pipestatus $last_status "[" "] " "|" (set_color $fish_color_status) \
                                  (set_color --bold $fish_color_status) $last_pipestatus)

        printf '[%s] %s%s@%s %s%s %s%s%s \f\r> ' (date "+%H:%M:%S") (set_color brblue) \
            $USER (prompt_hostname) (set_color $fish_color_cwd) $PWD $pipestatus_string \
            (set_color normal)
    end
end
