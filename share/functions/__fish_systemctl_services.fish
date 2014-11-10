function __fish_systemctl_services
    if type -q systemctl
        systemctl list-unit-files --no-legend --type=service ^/dev/null | cut -f 1 -d ' '
    end
end
