complete -c pactree -xa "(__fish_print_pacman_packages --installed)"
complete -c pactree -s b -l dbpath -d 'Set an alternate database location' -xa '(__fish_complete_directories)'
complete -c pactree -s c -l color -d 'Colorize output'
complete -c pactree -s d -l depth -d 'Limit the depth of recursion' -x
complete -c pactree -s g -l graph -d 'Generate output for graphviz\'s dot'
complete -c pactree -s h -l help -d 'Display this help message'
complete -c pactree -s l -l linear -d 'Enable linear output'
complete -c pactree -s r -l reverse -d 'Show reverse dependencies'
complete -c pactree -s s -l sync -d 'Search sync DBs instead of local'
complete -c pactree -s u -l unique -d 'Show dependencies with no duplicates (implies -l)'
complete -c pactree -l config -d 'Set an alternate configuration file' -r
