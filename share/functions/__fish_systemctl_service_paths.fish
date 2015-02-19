function __fish_systemctl_service_paths
	if type -q systemctl
		if __fish_contains_opt user
			systemctl --user list-unit-files --no-legend --type=path ^/dev/null $argv | cut -f 1 -d ' '
		else
			systemctl list-unit-files --no-legend --type=path ^/dev/null $argv | cut -f 1 -d ' '
		end
	end
end
