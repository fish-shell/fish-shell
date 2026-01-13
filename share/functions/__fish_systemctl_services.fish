# localization: skip(private)
function __fish_systemctl_services
    if not type -q systemctl
        return
    end

    # We don't want to complete with ANSI color codes
    set -lu SYSTEMD_COLORS

    # use the user specified mode, also exclude user defined alias
    set -l tokens (commandline -xp)
    # for determining whether user is using `journalctl`
    set -l command $tokens[1]
    set -l mode
    for t in $tokens[2..]
        switch $t
            case --system --user
                set mode $t
            case --global
                # `journalctl` does not have `global` option
                if test "$command" != journalctl
                    set mode $t
                end
            case --
                break
        end
    end

    set -l common_args --full --no-legend --no-pager --plain --type=service
    begin
        # `list-unit-files` will also show disabled units
        systemctl $mode list-unit-files $common_args
        # `list-units`      will not show disabled units but will show instances (like wpa_supplicant@wlan0.service)
        systemctl $mode list-units --state=loaded $common_args
    end 2>/dev/null | string split -f 1 ' '
end
