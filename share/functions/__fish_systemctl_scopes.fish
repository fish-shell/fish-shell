function __fish_systemctl_scopes
    if type -q systemctl
        if __fish_contains_opt user
            # Scopes are usually generated at runtime
            # Therefore show known _units_, not unit-files
            systemctl --user list-units --no-legend --type=scope ^/dev/null | cut -f 1 -d ' '
        else
            systemctl list-units --no-legend --type=scope ^/dev/null | cut -f 1 -d ' '
        end
    end
end
