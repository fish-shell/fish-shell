#completion for pkg_delete

# magic completion safety check (do not remove this comment)
if not type -q pkg_delete
    exit
end


complete -c pkg_delete -a '(__fish_print_packages)' --description 'Package'

complete -c pkg_delete -o a -d 'Delete unsed deps'
complete -c pkg_delete -o V -d 'Turn on stats'

