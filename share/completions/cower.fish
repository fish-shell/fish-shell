# Command specific completions for cower (an Archlinux AUR helper).
# Written by SanskritFritz (gmail)

complete -c cower -f -s b -l 'brief'            -d 'Show output in a more script friendly format'
complete -c cower -f -s d -l 'download'         -d 'Download [twice to fetch dependencies]'
complete -c cower -f -s i -l 'info'             -d 'Show info for target [twice for more details]'
complete -c cower -f -s m -l 'msearch'          -d 'Search for packages by maintainer'
complete -c cower -f -s s -l 'search'           -d 'Search for packages by name'
complete -c cower -f -s u -l 'update'           -d 'Check AUR packages for updates'
complete -c cower -f -s c -l 'color' -xa 'always auto never' -d 'Use colored output'
complete -c cower -f -s v -l 'debug'            -d 'Show debug output'
complete -c cower -f -s f -l 'force'            -d 'Overwrite existing files when downloading'
complete -c cower -f      -l 'format'           -d 'Print formatted'
complete -c cower -f -s h -l 'help'             -d 'Display help and quit'
complete -c cower -f      -l 'ignore' -xa "(pacman -Qq)" -d 'Ignore a package upgrade'
complete -c cower -f      -l 'ignorerepo' -xa "(cat /etc/pacman.conf | grep '^\[.\+\]' | sed 's/[]\[]//g')" -d 'Ignore a binary repo when checking for updates'
complete -c cower -f      -l 'listdelim'        -d 'Specify a delimiter for list formatters'
complete -c cower -f      -l 'nossl'            -d 'No secure http connections to the AUR'
complete -c cower -f -s q -l 'quiet'            -d 'Output less'
complete -c cower -f -s t -l 'target'           -d 'Download targets to DIR'
complete -c cower -f      -l 'threads'          -d 'Limit the number of threads created [10]'
complete -c cower -f      -l 'timeout'          -d 'Curl timeout in seconds'
complete -c cower -f -s v -l 'verbose'          -d 'Output more'

# Complete with AUR packages:
complete -c cower -f --condition 'not expr -- (commandline --current-token) : "^\-.*" > /dev/null' --arguments '(cower --format="%n\n" --search (commandline --current-token))'
