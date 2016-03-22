
function __fish_print_hostnames -d "Print a list of known hostnames"
	# HACK: This only deals with ipv4

	# Print all hosts from /etc/hosts
	if type -q getent
		# Ignore zero ips
		getent hosts | grep -v '^0.0.0.0' \
		| string replace -r '[0-9.]*\s*' '' | string split " "
	else if test -r /etc/hosts
		# Ignore commented lines and functionally empty lines
		grep -v '^\s*0.0.0.0\|^\s*#\|^\s*$' /etc/hosts \
		# Strip comments
		| string replace -ra '#.*$' '' \
		| string replace -r '[0-9.]*\s*' '' | string trim | string replace -ra '\s+' '\n'
	end

	# Print nfs servers from /etc/fstab
	if test -r /etc/fstab
		string match -r '^\s*[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3]:|^[a-zA-Z\.]*:' </etc/fstab | string replace -r ':.*' ''
	end

	# Check hosts known to ssh
	set -l known_hosts ~/.ssh/known_hosts{,2} /etc/ssh/known_hosts{,2} # Yes, seriously - the default specifies both with and without "2"
	for file in /etc/ssh/ssh_config ~/.ssh/config
		if test -r $file
			# Print hosts from system wide ssh configuration file
			# Note the non-capturing group to avoid printing "name"
			string match -ri '\s*Host(?:name)? \w.*' < $file | string replace -ri '^\s*Host(?:name)?\s*(\S+)' '$1'
			set known_hosts $known_hosts (string match -ri '^\s*UserKnownHostsFile|^\s*GlobalKnownHostsFile' <$file \
			| string replace -ri '.*KnownHostsFile\s*' '')
		end
	end
	for file in $known_hosts
		# Ignore hosts that are hashed, commented or have custom ports (like [localhost]:2200)
		test -r $file; and string replace -ra '(\S+) .*' '$1' < $file | string match -r '^[^#|[=]+$' | string split ","
	end
	return 0
end
