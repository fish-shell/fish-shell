# name: Informative
# http://michal.karzynski.pl/blog/2009/11/19/my-informative-shell-prompt/


function fish_prompt --description 'Write out the prompt'
    #Save the return status of the previous command
    set stat $status

# Just calculate these once, to save a few cycles when displaying the prompt
    if not set -q __fish_prompt_hostname
        set -g __fish_prompt_hostname (hostname|cut -d . -f 1)
    end

if not set -q __fish_prompt_normal
        set -g __fish_prompt_normal (set_color normal)
    end

if not set -q __fish_color_blue
        set -g __fish_color_blue (set_color -o blue)
    end

#Set the color for the status depending on the value
    set __fish_color_status (set_color -o green)
    if test $stat -gt 0
        set __fish_color_status (set_color -o red)
    end

switch $USER

case root toor

if not set -q __fish_prompt_cwd
            if set -q fish_color_cwd_root
                set -g __fish_prompt_cwd (set_color $fish_color_cwd_root)
            else
                set -g __fish_prompt_cwd (set_color $fish_color_cwd)
            end
        end

printf '%s@%s %s%s%s# ' $USER $__fish_prompt_hostname "$__fish_prompt_cwd" (prompt_pwd) "$__fish_prompt_normal"

case '*'

if not set -q __fish_prompt_cwd
            set -g __fish_prompt_cwd (set_color $fish_color_cwd)
        end

printf '[%s] %s%s@%s %s%s %s(%s)%s \f\r> ' (date "+%H:%M:%S") "$__fish_color_blue" $USER $__fish_prompt_hostname "$__fish_prompt_cwd" "$PWD" "$__fish_color_status" "$stat" "$__fish_prompt_normal"

end
end
