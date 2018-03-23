function __fish_portage_print_installed_categories --description 'Print all installed categories'
	find /var/db/pkg -mindepth 1 -maxdepth 1 -type d -printf '%P\n'
end
