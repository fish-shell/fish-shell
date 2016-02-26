
function __fish_print_hostnames -d "Print a list of known hostnames"
	# HACK: This only deals with ipv4

	# Ignore zero ips
	# An option like grep's "-v" would make this massively simpler
	# It can't be grouped because that would lead to one line for the entire thing and one per-group
	set -l renonzero '^[^0].+|^0.[^0].+|^0.0.[^0].+|^0.0.0.[^0].+'
	# Print all hosts from /etc/hosts
	if type -q getent
		getent hosts | string match -r $renonzero \
		| string replace -r '[0-9.]*\s*' '' | string split " "
	else if test -r /etc/hosts
		# Ignore commented lines
		string match -r '^[^#\s]+ .*$' </etc/hosts | string match -r $renonzero \
		| string replace -r '[0-9.]*\s*' '' | string split " "
	end
	# Print nfs servers from /etc/fstab
	if test -r /etc/fstab
		string match -r '^\s*[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3]:|[a-zA-Z\.]*:' </etc/fstab | string replace -r ':.*' ''
	end

	# Print hosts with known ssh keys
	# Does not match hostnames with @directives specified

	# Print hosts from system wide ssh configuration file
	if [ -e /etc/ssh/ssh_config ]
		awk -v FS="[ =]+" -v OFS='\n' 'tolower($0) ~ /^ *host[^*?!]*$/{ $1=""; print }' /etc/ssh/ssh_config
	end

	# Print hosts from ssh configuration file
	if [ -e ~/.ssh/config ]
		awk -v FS="[ =]+" -v OFS='\n' 'tolower($0) ~ /^ *host[^*?!]*$/{ $1=""; print }' ~/.ssh/config
	end

	# Check external ssh KnownHostsFiles
	set -l known_hosts ~/.ssh/known_hosts{,2} /etc/ssh/known_hosts{,2} # Yes, seriously - the default specifies both with and without "2"
	for file in /etc/ssh/ssh_config ~/.ssh/config
		if test -r $file
			set known_hosts $known_hosts (string match -r '^\s*UserKnownHostsFile|^\s*GlobalKnownHostsFile' <$file \
			| string replace -r '.*KnownHostsFile\s*' '')
		end
	end
	for file in $known_hosts
		test -r $file; and string replace -ra '(\S+) .*' '$1' <$file | string replace -ra '^\[([0-9.]+)\]:[0-9]+$' '$1' | string split ","
	end
end
