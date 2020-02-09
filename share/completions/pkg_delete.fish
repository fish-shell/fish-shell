#completion for pkg_delete


complete -c pkg_delete -a '(__fish_print_packages)' --description 'Package'

complete -c pkg_delete -o a -d 'Delete unused deps'
complete -c pkg_delete -o V -d 'Turn on stats'

