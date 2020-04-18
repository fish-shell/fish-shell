function __fish_systemctl_services
    if type -q systemctl
        if __fish_contains_opt user
            systemctl --user list-unit-files --full --no-legend --no-pager --plain --type=service 2>/dev/null $argv | string split -f 1 ' '
            systemctl --user list-units --state=loaded --full --no-legend --no-pager --plain --type=service 2>/dev/null | string split -f 1 ' '
        else
            # list-unit-files will also show disabled units
            systemctl list-unit-files --full --no-legend --no-pager --plain --type=service 2>/dev/null $argv | string split -f 1 ' '
            # list-units will not show disabled units but will show instances (like wpa_supplicant@wlan0.service)
            systemctl list-units --state=loaded --full --no-legend --no-pager --plain --type=service 2>/dev/null | string split -f 1 ' '
        end
    end
end
