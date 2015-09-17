function __fish_systemctl_services
	if type -q systemctl
		if __fish_contains_opt user
			systemctl --user list-unit-files --no-legend --type=service ^/dev/null $argv | cut -f 1 -d ' '
			systemctl --user list-units --state=loaded --no-legend --type=service ^/dev/null | cut -f 1 -d ' '
		else
			# list-unit-files will also show disabled units
			systemctl list-unit-files --no-legend --type=service ^/dev/null $argv | cut -f 1 -d ' '
			# list-units will not show disabled units but will show instances (like wpa_supplicant@wlan0.service)
			systemctl list-units --state=loaded --no-legend --type=service ^/dev/null | cut -f 1 -d ' '
		end
	end
end
