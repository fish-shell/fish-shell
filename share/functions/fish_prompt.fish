# name: Default
# author: Lily Ballard

function ghoti_prompt --description 'Write out the prompt'
    set -l last_pipestatus $pipestatus
    set -lx __ghoti_last_status $status # Export for __ghoti_print_pipestatus.
    set -l normal (set_color normal)
    set -q ghoti_color_status
    or set -g ghoti_color_status red

    # Color the prompt differently when we're root
    set -l color_cwd $ghoti_color_cwd
    set -l suffix '>'
    if functions -q ghoti_is_root_user; and ghoti_is_root_user
        if set -q ghoti_color_cwd_root
            set color_cwd $ghoti_color_cwd_root
        end
        set suffix '#'
    end

    # Write pipestatus
    # If the status was carried over (if no command is issued or if `set` leaves the status untouched), don't bold it.
    set -l bold_flag --bold
    set -q __ghoti_prompt_status_generation; or set -g __ghoti_prompt_status_generation $status_generation
    if test $__ghoti_prompt_status_generation = $status_generation
        set bold_flag
    end
    set __ghoti_prompt_status_generation $status_generation
    set -l status_color (set_color $ghoti_color_status)
    set -l statusb_color (set_color $bold_flag $ghoti_color_status)
    set -l prompt_status (__ghoti_print_pipestatus "[" "]" "|" "$status_color" "$statusb_color" $last_pipestatus)

    echo -n -s (prompt_login)' ' (set_color $color_cwd) (prompt_pwd) $normal (ghoti_vcs_prompt) $normal " "$prompt_status $suffix " "
end
