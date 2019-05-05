function __fish_portage_print_installed_pkgs --description 'Print all installed packages (non-deduplicated)'
    find /var/db/pkg -mindepth 2 -maxdepth 2 -type d -printf '%P\n' | string replace -r -- '-[0-9][0-9.]*.*$' ''
end
