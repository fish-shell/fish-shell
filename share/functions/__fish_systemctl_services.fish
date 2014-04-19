function __fish_systemctl_services
    command find /etc/systemd/system -type f -name '*.service' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.service' -printf '%f\n'
end
