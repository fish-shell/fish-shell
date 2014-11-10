function __fish_systemctl_mounts
    if type -q systemctl
        systemctl list-unit-files --no-legend --type=mount ^/dev/null | cut -f 1 -d ' '
    end
end
