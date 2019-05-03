function __fish_portage_print_available_pkgs --description 'Print all available packages'
    find (__fish_portage_print_repository_paths) -mindepth 2 -maxdepth 2 -type d ! '(' '(' -path '*/eclass/*' -o -path '*/metadata/*' -o -path '*/profiles/*' -o -path '*/.*/*' ')' -prune ')' -printf '%P\n'
end
