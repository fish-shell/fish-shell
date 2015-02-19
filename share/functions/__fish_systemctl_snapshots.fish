function __fish_systemctl_snapshots
	if type -q systemctl
		if __fish_contains_opt user
			# Snapshots are usually generated at runtime
			# Therefore show known _units_, not unit-files
			# They are also often not loaded, so add "--all"
			systemctl --user list-units --all --no-legend --type=snapshot ^/dev/null | cut -f 1 -d ' '
		else
			systemctl list-units --all --no-legend --type=snapshot ^/dev/null | cut -f 1 -d ' '
		end
	end
end
