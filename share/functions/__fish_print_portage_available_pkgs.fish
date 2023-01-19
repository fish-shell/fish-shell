function __fish_print_portage_available_pkgs --description 'Print all available packages'
    set -l paths (__fish_print_portage_repository_paths)
    set -q paths[1]
    or return
    find $paths -mindepth 2 -maxdepth 2 -type d ! '(' '(' -path '*/eclass/*' -o -path '*/metadata/*' -o -path '*/profiles/*' -o -path '*/.*/*' ')' -prune ')' -printf '%P\n' 2>/dev/null
end
