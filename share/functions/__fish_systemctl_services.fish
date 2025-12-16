# localization: skip(private)
function __fish_systemctl_services
    if not type -q systemctl
        return
    end

    # We don't want to complete with ANSI color codes
    set -lu SYSTEMD_COLORS

    set -l common_args --full --no-legend --no-pager --plain --type=service
    if __fish_contains_opt user
        systemctl --user list-unit-files $common_args
        systemctl --user list-units --state=loaded $common_args
    else
        # list-unit-files will also show disabled units
        systemctl list-unit-files $common_args
        # list-units will not show disabled units but will show instances (like wpa_supplicant@wlan0.service)
        systemctl list-units --state=loaded $common_args
    end 2>/dev/null | string split -f 1 ' '
end
