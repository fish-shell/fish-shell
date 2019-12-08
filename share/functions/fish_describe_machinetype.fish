function fish_describe_machinetype
    # Check what kind of "machine" we think fish is running on,
    # whether that be remote via ssh or a container or vm (or multiple).
    #
    # This depends on tools to give us the info,
    # and VMs can also just hide from us, so it isn't good enough
    # to use to e.g. hide the user@host info in a prompt.

    # We cache this because it doesn't change.
    if not set -q __fish_machine_type
        set -g __fish_machine_type

        if set -q SSH_TTY
            set -a __fish_machine_type ssh
        end

        if command -sq systemd-detect-virt
            # This prints "none" on local bare metal, but it then also returns 1.
            set -l sdv (systemd-detect-virt)
            and set -a __fish_machine_type $sdv
        end

        if test -r /etc/debian_chroot
            or begin command -sq ischroot
                and ischroot 2>/dev/null
            end
            set -a __fish_machine_type chroot
        end
    end

    string join \n $__fish_machine_type

    # Return 0 if we have something
    set -q __fish_machine_type[1]
end
