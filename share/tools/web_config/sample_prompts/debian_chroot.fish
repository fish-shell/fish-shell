# name: Debian chroot
# author: Maurizio De Santis

function fish_prompt --description 'Write out the prompt, prepending the Debian chroot environment if present'
    # Set variable identifying the chroot you work in (used in the prompt below)
    set -l debian_chroot $debian_chroot

    if not set -q debian_chroot[1]
        and test -r /etc/debian_chroot
        set debian_chroot (cat /etc/debian_chroot)
    end

    if not set -q __fish_debian_chroot_prompt
        and set -q debian_chroot[1]
        and test -n "$debian_chroot"
        set -g __fish_debian_chroot_prompt "($debian_chroot)"
    end

    # Prepend the chroot environment if present
    if set -q __fish_debian_chroot_prompt
        echo -n -s (set_color yellow) "$__fish_debian_chroot_prompt" (set_color normal) ' '
    end

    if functions -q fish_is_root_user; and fish_is_root_user
        echo -n -s "$USER" @ (prompt_hostname) ' ' (set -q fish_color_cwd_root
                                                    and set_color $fish_color_cwd_root
                                                    or set_color $fish_color_cwd) (prompt_pwd) \
            (set_color normal) '# '

    else
        echo -n -s "$USER" @ (prompt_hostname) ' ' (set_color $fish_color_cwd) (prompt_pwd) \
            (set_color normal) '> '

    end
end
