# It seems machinectl will eliminate spaces from machine names so we don't need to handle that
function __fish_systemd_machines
	machinectl --no-legend --no-pager list --all | while read -l a b; echo $a; end
end
