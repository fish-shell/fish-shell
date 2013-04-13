
function __fish_print_hostnames -d "Print a list of known hostnames"

	# Print all hosts from /etc/hosts
	if test -x /usr/bin/getent
		getent hosts | tr -s ' ' ' ' | cut -d ' ' -f 2- | tr ' ' '\n'
	elseif test -f /etc/hosts
		tr -s ' \t' '  ' < /etc/hosts | sed 's/ *#.*//' | cut -s -d ' ' -f 2- | sgrep -o '[^ ]*'
	end
	# Print nfs servers from /etc/fstab
	if test -f /etc/fstab
		sgrep </etc/fstab "^\([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*\|[a-zA-Z.]*\):"|cut -d : -f 1
	end

	# Print hosts with known ssh keys
	cat ~/.ssh/known_hosts{,2} ^/dev/null|cut -d ' ' -f 1| cut -d , -f 1

	# Print hosts from ssh configuration file
	if [ -e ~/.ssh/config ]
		sgrep '^ *Host' ~/.ssh/config | grep -v '[*?]' | cut -d ' ' -f 2
	end
end

