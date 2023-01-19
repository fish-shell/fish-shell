function __fish_print_portage_installed_pkgs --description 'Print all installed packages (non-deduplicated)'
    find /var/db/pkg -mindepth 2 -maxdepth 2 -type d -printf '%P\n' 2>/dev/null | string replace -r -- '-[0-9][0-9.]*.*$' ''
end
