function __fish_portage_print_repository_names --description 'Print the names of all configured repositories'
	# repos.conf may be a file or a directory
	find /etc/portage/repos.conf -type f -exec cat '{}' + | string replace -r --filter '^\s*\[([[:alnum:]_][[:alnum:]_-]*)\]' '$1' | string match -v -e DEFAULT
end
