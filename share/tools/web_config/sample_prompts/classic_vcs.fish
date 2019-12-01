# name: Classic + Vcs
# author: Lily Ballard
# vim: set noet:

function fish_prompt --description 'Write out the prompt'
    set -l last_pipestatus $pipestatus
    set -l last_status $status
    set -l normal (set_color normal)

    # initialize our new variables
    if not set -q __fish_classic_git_prompt_initialized
        set -qU fish_color_user
        or set -U fish_color_user -o green
        set -qU fish_color_host
        or set -U fish_color_host -o cyan
        set -qU fish_color_status
        or set -U fish_color_status red
        set -U __fish_classic_git_prompt_initialized
    end

    set -l color_cwd
    set -l prefix
    set -l suffix
    switch "$USER"
        case root toor
            if set -q fish_color_cwd_root
                set color_cwd $fish_color_cwd_root
            else
                set color_cwd $fish_color_cwd
            end
            set suffix '#'
        case '*'
            set color_cwd $fish_color_cwd
            set suffix '>'
    end

    set -l prompt_status (__fish_print_pipestatus "[" "] " "|" (set_color yellow) (set_color --bold yellow) $last_pipestatus)
    if test $last_status -ne 0
        set prompt_status " $prompt_status" (set_color $fish_color_status) "[$last_status]" "$normal"
    end

    echo -n -s (set_color $fish_color_user) "$USER" $normal @ (set_color $fish_color_host) (prompt_hostname) $normal ' ' (set_color $color_cwd) (prompt_pwd) $normal (fish_vcs_prompt) $normal $prompt_status $suffix " "
end
