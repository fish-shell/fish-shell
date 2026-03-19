# localization: skip(private)
function __fish_print_portage_repository_paths --description 'Print the paths of all configured repositories'
    set -l eroot (portageq envvar EROOT)
    set -l repos (portageq get_repos $eroot | xargs printf "%s\n")
    portageq get_repo_path $eroot $repos
end
