function __fish_systemctl_sockets
    command find /etc/systemd/system -type f -name '*.socket' -printf '%f\n'
    command find /usr/lib/systemd/system -type f -name '*.socket' -printf '%f\n'
end
