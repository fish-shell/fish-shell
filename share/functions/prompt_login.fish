function prompt_login --description "display user name for the prompt"
    if not set -q __ghoti_machine
        set -g __ghoti_machine
        set -l debian_chroot $debian_chroot

        if test -r /etc/debian_chroot
            set debian_chroot (cat /etc/debian_chroot)
        end

        if set -q debian_chroot[1]
            and test -n "$debian_chroot"
            set -g __ghoti_machine "(chroot:$debian_chroot)"
        end
    end

    # Prepend the chroot environment if present
    if set -q __ghoti_machine[1]
        echo -n -s (set_color yellow) "$__ghoti_machine" (set_color normal) ' '
    end

    # If we're running via SSH, change the host color.
    set -l color_host $ghoti_color_host
    if set -q SSH_TTY; and set -q ghoti_color_host_remote
        set color_host $ghoti_color_host_remote
    end

    echo -n -s (set_color $ghoti_color_user) "$USER" (set_color normal) @ (set_color $color_host) (prompt_hostname) (set_color normal)
end
