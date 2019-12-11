# name: Classic + Vcs
# author: Lily Ballard

function fish_prompt --description 'Write out the prompt'
    set -l last_pipestatus $pipestatus
    set -l normal (set_color normal)

    # Color the prompt differently when we're root
    set -l color_cwd $fish_color_cwd
    set -l prefix
    set -l suffix '>'
    if contains -- $USER root toor
            if set -q fish_color_cwd_root
                set color_cwd $fish_color_cwd_root
            end
            set suffix '#'
    end

    # Write pipestatus
    set -l prompt_status (__fish_print_pipestatus " [" "]" "|" (set_color $fish_color_status) (set_color --bold $fish_color_status) $last_pipestatus)

    set -q fish_color_host_ssh
    or set -U fish_color_host_ssh yellow

    # If we're connected via ssh, color the host differently.
    set -l hostcolor $fish_color_host
    set -l machinetype (fish_describe_machinetype)
    if contains -- ssh $machinetype
        set hostcolor $fish_color_host_ssh
    end

    echo -n -s (set_color $fish_color_user) "$USER" $normal @ (set_color $hostcolor) (prompt_hostname) $normal ' ' (set_color $color_cwd) (prompt_pwd) $normal (fish_vcs_prompt) $normal $prompt_status $suffix " "
end
