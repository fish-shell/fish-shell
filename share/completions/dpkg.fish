
complete -c dpkg -s i -l install -d 'Install .deb package'     -xa '(__fish_complete_suffix .deb)'
complete -c dpkg -s r -l remove  -d 'Remove package'           -xa '(__fish_print_packages)'
complete -c dpkg -s P -l purge   -d 'Purge package'            -xa '(__fish_print_packages)'
complete -c dpkg -l force-all    -d 'Continue on all problems'

