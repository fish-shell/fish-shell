# name: Informative Vcs
# author: Mariusz Smykula <mariuszs at gmail.com>

function ghoti_prompt --description 'Write out the prompt'
    set -l last_pipestatus $pipestatus
    set -lx __ghoti_last_status $status # Export for __ghoti_print_pipestatus.

    if not set -q __ghoti_git_prompt_show_informative_status
        set -g __ghoti_git_prompt_show_informative_status 1
    end
    if not set -q __ghoti_git_prompt_hide_untrackedfiles
        set -g __ghoti_git_prompt_hide_untrackedfiles 1
    end
    if not set -q __ghoti_git_prompt_color_branch
        set -g __ghoti_git_prompt_color_branch magenta --bold
    end
    if not set -q __ghoti_git_prompt_showupstream
        set -g __ghoti_git_prompt_showupstream informative
    end
    if not set -q __ghoti_git_prompt_color_dirtystate
        set -g __ghoti_git_prompt_color_dirtystate blue
    end
    if not set -q __ghoti_git_prompt_color_stagedstate
        set -g __ghoti_git_prompt_color_stagedstate yellow
    end
    if not set -q __ghoti_git_prompt_color_invalidstate
        set -g __ghoti_git_prompt_color_invalidstate red
    end
    if not set -q __ghoti_git_prompt_color_untrackedfiles
        set -g __ghoti_git_prompt_color_untrackedfiles $ghoti_color_normal
    end
    if not set -q __ghoti_git_prompt_color_cleanstate
        set -g __ghoti_git_prompt_color_cleanstate green --bold
    end

    set -l color_cwd
    set -l suffix
    if functions -q ghoti_is_root_user; and ghoti_is_root_user
        if set -q ghoti_color_cwd_root
            set color_cwd $ghoti_color_cwd_root
        else
            set color_cwd $ghoti_color_cwd
        end
        set suffix '#'
    else
        set color_cwd $ghoti_color_cwd
        set suffix '$'
    end

    # PWD
    set_color $color_cwd
    echo -n (prompt_pwd)
    set_color normal

    printf '%s ' (ghoti_vcs_prompt)

    set -l status_color (set_color $ghoti_color_status)
    set -l statusb_color (set_color --bold $ghoti_color_status)
    set -l prompt_status (__ghoti_print_pipestatus "[" "]" "|" "$status_color" "$statusb_color" $last_pipestatus)
    echo -n $prompt_status
    set_color normal

    echo -n "$suffix "
end
