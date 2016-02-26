
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
		__fish_sgrep </etc/fstab "^\([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*\|[a-zA-Z.]*\):"|cut -d : -f 1
	end

	# Print hosts with known ssh keys
	# Does not match hostnames with @directives specified
	__fish_sgrep -Eoh '^[^#@|, ]*' ~/.ssh/known_hosts{,2} ^/dev/null | string replace -r '^\[([^]]+)\]:[0-9]+$' '$1'

	# Print hosts from system wide ssh configuration file
	if [ -e /etc/ssh/ssh_config ]
		awk -v FS="[ =]+" -v OFS='\n' 'tolower($0) ~ /^ *host[^*?!]*$/{ $1=""; print }' /etc/ssh/ssh_config
	end

	# Print hosts from ssh configuration file
	if [ -e ~/.ssh/config ]
		awk -v FS="[ =]+" -v OFS='\n' 'tolower($0) ~ /^ *host[^*?!]*$/{ $1=""; print }' ~/.ssh/config
	end
end
