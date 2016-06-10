
complete -c dpkg -s i -l install -d 'Install .deb package'     -xa '(__fish_complete_suffix .deb)'
complete -c dpkg      -l unpack  -d 'Unpack .deb package'      -xa '(__fish_complete_suffix .deb)'
complete -c dpkg      -l configure -d 'Configure package'      -xa '(dpkg-query -W -f \'${Package}\n\')'
complete -c dpkg -s r -l remove  -d 'Remove package'           -xa '(dpkg-query -W -f \'${Package}\n\')'
complete -c dpkg -s P -l purge   -d 'Purge package'            -xa '(dpkg-query -W -f \'${Package}\n\')'
complete -c dpkg -s V -l verify  -d 'Verify contents of package' -xa '(dpkg-query -W -f \'${Package}\n\')'
complete -c dpkg -l force-all    -d 'Continue on all problems'

# dpkg-deb options
complete -c dpkg -s b -l build    -d 'Build package from directory' -xa '(__fish_complete_directories (commandline -ct))'
complete -c dpkg -s c -l contents -d 'List contents of .deb'        -xa '(__fish_complete_suffix .deb)'
complete -c dpkg -s I -l info     -d 'Show .deb information'        -xa '(__fish_complete_suffix .deb)'

# dpkg-query options
complete -c dpkg -s l -l list      -d 'List packages matching pattern' -xa '(dpkg-query -W -f \'${Package}\n\')'
complete -c dpkg -s L -l listfiles -d 'List contents of packages'      -xa '(dpkg-query -W -f \'${Package}\n\')'
complete -c dpkg -s s -l status    -d 'Print status of package'        -xa '(dpkg-query -W -f \'${Package}\n\')'
complete -c dpkg -s S -l search    -d 'Search for packages containing file'
