function __fish_portage_print_installed_pkgs --description 'Print all installed packages'
	find /var/db/pkg -mindepth 2 -maxdepth 2 -type d -printf '%P\n' | sed 's/-[0-9][0-9.]*.*$//' | sort -u
end
