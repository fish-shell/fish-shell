
function __fish_print_hostnames -d "Print a list of known hostnames"

	# Print all hosts from /etc/hosts
	if test -f /etc/hosts
		sed </etc/hosts -e 's/[0-9.]*\( \|\t\)*\(.*\)/\2/'|sed -e 's/\#.*//'|tr \t \n |sgrep -v '^$'
	end
	# Print nfs servers from /etc/fstab
	if test -f /etc/fstab
		sgrep </etc/fstab "^\([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*\|[a-zA-Z.]*\):"|cut -d : -f 1
	end

	# Print hosts with known ssh keys
	cat ~/.ssh/known_hosts{,2} ^/dev/null|cut -d ' ' -f 1| cut -d , -f 1
end

