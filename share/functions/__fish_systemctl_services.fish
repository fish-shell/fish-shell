function __fish_systemctl_services
    if type -q systemctl
        if __fish_contains_opt user
            systemctl --user list-unit-files --no-legend --type=service 2>/dev/null $argv | while read -l line
                string split -f 1 ' ' -- $line
            end
            systemctl --user list-units --state=loaded --no-legend --type=service 2>/dev/null | while read -l line
                string split -f 1 ' ' -- $line
            end
        else
            # list-unit-files will also show disabled units
            systemctl list-unit-files --no-legend --type=service 2>/dev/null $argv | while read -l line
                string split -f 1 ' ' -- $line
            end
            # list-units will not show disabled units but will show instances (like wpa_supplicant@wlan0.service)
            systemctl list-units --state=loaded --no-legend --type=service 2>/dev/null | while read -l line
                string split -f 1 ' ' -- $line
            end
        end
    end
end
