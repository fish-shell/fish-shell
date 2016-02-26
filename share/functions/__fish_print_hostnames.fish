
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
		string match -r '^\s*[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3]:|[a-zA-Z\.]*:' </etc/fstab | string replace -r ':.*' ''
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
