function __fish_systemctl_automounts
    if type -q systemctl
        if __fish_contains_opt user
            systemctl --user list-unit-files --no-legend --type=automount ^/dev/null | cut -f 1 -d ' '
        else
            systemctl list-unit-files --no-legend --type=automount ^/dev/null | cut -f 1 -d ' '
        end
    end
end
