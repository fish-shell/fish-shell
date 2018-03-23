function __fish_portage_print_repository_names --description 'Print the names of all configured repositories'
	# repos.conf may be a file or a directory
	find /etc/portage/repos.conf -type f -exec sed -ne '/^[[:blank:]]*\[[[:alnum:]_][[:alnum:]_-]*\]/{s!^.*\[\(.*\)\].*$!\1!; /DEFAULT/!p}' '{}' +
end
