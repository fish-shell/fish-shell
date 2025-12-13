# localization: skip(private)
function __fish_systemctl_services
    # We don't want to complete with ANSI color codes
    set -lu SYSTEMD_COLORS

    if not type -q systemctl
        return
    end

    # use the user specified mode, also exclude user defined alias
    set -l tokens (commandline -xp)
    set -l cmdline $tokens[1]
    set -l mode
    for t in $tokens
        switch $t
            case --system --user
                set mode $t
            case --global
                # journalctl does not have `global` option
                if test "$cmdline" != journalctl
                    set mode $t
                end
            case --
                break
        end
    end

    # list-unit-files will also show disabled units
    systemctl $mode list-unit-files --full --no-legend --no-pager --plain --type=service 2>/dev/null $argv | string split -f 1 ' '
    # list-units will not show disabled units but will show instances (like wpa_supplicant@wlan0.service)
    systemctl $mode list-units --state=loaded --full --no-legend --no-pager --plain --type=service 2>/dev/null | string split -f 1 ' '
end
