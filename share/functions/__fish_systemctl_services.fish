function __fish_systemctl_services
	if type -q systemctl
		if __fish_contains_opt user
			systemctl --user list-unit-files --no-legend --type=service ^/dev/null | cut -f 1 -d ' '
		else
			systemctl list-unit-files --no-legend --type=service ^/dev/null | cut -f 1 -d ' '
		end
	end
end
