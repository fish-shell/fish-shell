# localization: skip(private)
#
# It seems machinectl will eliminate spaces from machine names so we don't need to handle that
function __fish_systemd_machines
    # We don't want to complete with ANSI color codes
    set -lu SYSTEMD_COLORS

    machinectl --no-legend --no-pager list --all | while read -l a b
        echo $a
    end
end
