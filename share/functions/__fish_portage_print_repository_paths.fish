function __fish_portage_print_repository_paths --description 'Print the paths of all configured repositories'
    # repos.conf may be a file or a directory
    find /etc/portage/repos.conf -type f -exec cat '{}' + | string replace -r --filter '^\s*location\s*=\s*(\S+)' '$1'
end
