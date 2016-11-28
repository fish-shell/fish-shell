function __fish_systemctl_slices
    if type -q systemctl
        if __fish_contains_opt user
            # Slices are usually generated at runtime
            # Therefore show known _units_, not unit-files
            systemctl --user list-units --no-legend --type=slice ^/dev/null | cut -f 1 -d ' '
        else
            systemctl list-units --no-legend --type=slice ^/dev/null | cut -f 1 -d ' '
        end
    end
end
