# localization: skip(private)
function __fish_print_portage_installed_pkgs --description 'Print all installed packages (non-deduplicated)'
    if type -f -q eix
        EIX_LIMIT=0 eix -I --only-names
    else
        find (portageq envvar vdb_path) -mindepth 2 -maxdepth 2 -type d -printf '%P\n' 2>/dev/null | string replace -r -- '-[0-9][0-9.]*.*$' ''
    end
end
