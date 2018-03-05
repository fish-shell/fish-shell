#completion for pkg_info

# magic completion safety check (do not remove this comment)
if not type -q pkg_info
    exit
end


complete -c pkg_info -a '(__fish_print_packages)' --description 'Package'

