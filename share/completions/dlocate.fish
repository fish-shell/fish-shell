complete -c dlocate -s S -x -d 'List records that match filenames'
complete -c dlocate -s L -d 'List all files in the package' -xa '(__fish_print_apt_packages)'
complete -c dlocate -o ls -d 'ls -ldF all files in the package' -xa '(__fish_print_apt_packages)'
complete -c dlocate -o du -d 'du -sck all files in the package' -xa '(__fish_print_apt_packages)'
