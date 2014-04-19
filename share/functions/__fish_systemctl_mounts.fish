function __fish_systemctl_mounts
    command find /etc/systemd/system -type f -name '*.mount' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.mount' -printf '%f\n'
end
