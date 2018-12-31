# name: Debian chroot
# author: Maurizio De Santis

function fish_prompt --description 'Write out the prompt, prepending the Debian chroot environment if present'
    if not set -q __fish_prompt_normal
        set -g __fish_prompt_normal (set_color normal)
    end

    if not set -q __fish_prompt_chroot_env
        set -g __fish_prompt_chroot_env (set_color yellow)
    end

    # Set variable identifying the chroot you work in (used in the prompt below)
    if not set -q debian_chroot
        and test -r /etc/debian_chroot
        set debian_chroot (cat /etc/debian_chroot)
    end
    if not set -q __fish_debian_chroot_prompt
        and set -q debian_chroot
        and test -n "$debian_chroot"
        set -g __fish_debian_chroot_prompt "($debian_chroot)"
    end

    # Prepend the chroot environment if present
    if set -q __fish_debian_chroot_prompt
        echo -n -s "$__fish_prompt_chroot_env" "$__fish_debian_chroot_prompt" "$__fish_prompt_normal" ' '
    end

    switch "$USER"
        case root toor
            if not set -q __fish_prompt_cwd
                if set -q fish_color_cwd_root
                    set -g __fish_prompt_cwd (set_color $fish_color_cwd_root)
                else
                    set -g __fish_prompt_cwd (set_color $fish_color_cwd)
                end
            end

            echo -n -s "$USER" @ (prompt_hostname) ' ' "$__fish_prompt_cwd" (prompt_pwd) "$__fish_prompt_normal" '# '

        case '*'
            if not set -q __fish_prompt_cwd
                set -g __fish_prompt_cwd (set_color $fish_color_cwd)
            end

            echo -n -s "$USER" @ (prompt_hostname) ' ' "$__fish_prompt_cwd" (prompt_pwd) "$__fish_prompt_normal" '> '

    end
end
