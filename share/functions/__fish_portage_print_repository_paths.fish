function __fish_portage_print_repository_paths --description 'Print the paths of all configured repositories'
	# repos.conf may be a file or a directory
	find /etc/portage/repos.conf -type f -exec awk 'BEGIN { FS="[[:blank:]]*=[[:blank:]]*" } $1 == "location" { print $2 }' '{}' +
end
