function __fish_systemctl_service_paths
    if type -q systemctl
        systemctl list-unit-files --no-legend --type=path ^/dev/null | cut -f 1 -d ' '
    end
end
