# name: Astronaut
# author: Radu Potop
# description: A prompt theme similar to Starship.rs but without the emojis.

function ghoti_prompt --description 'Write out the prompt'
    set -l last_status $status
    set -l normal (set_color normal)
    set -l status_color (set_color brgreen)
    set -l cwd_color (set_color $ghoti_color_cwd)
    set -l vcs_color (set_color brpurple)
    set -l prompt_status ""

    # Since we display the prompt on a new line allow the directory names to be longer.
    set -q ghoti_prompt_pwd_dir_length
    or set -lx ghoti_prompt_pwd_dir_length 0

    # Color the prompt differently when we're root
    set -l suffix '❯'
    if functions -q ghoti_is_root_user; and ghoti_is_root_user
        if set -q ghoti_color_cwd_root
            set cwd_color (set_color $ghoti_color_cwd_root)
        end
        set suffix '#'
    end

    # Color the prompt in red on error
    if test $last_status -ne 0
        set status_color (set_color $ghoti_color_error)
        set prompt_status $status_color "[" $last_status "]" $normal
    end

    echo -s (prompt_login) ' ' $cwd_color (prompt_pwd) $vcs_color (ghoti_vcs_prompt) $normal ' ' $prompt_status
    echo -n -s $status_color $suffix ' ' $normal
end
