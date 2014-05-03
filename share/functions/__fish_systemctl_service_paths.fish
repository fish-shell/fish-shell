function __fish_systemctl_service_paths
    command find /etc/systemd/system -type f -name '*.path' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.path' -printf '%f\n'
end
