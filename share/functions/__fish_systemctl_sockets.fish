function __fish_systemctl_sockets
    if type -q systemctl
        systemctl list-unit-files --no-legend --type=socket ^/dev/null | cut -f 1 -d ' '
    end
end
