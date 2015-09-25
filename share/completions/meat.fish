# Command specific completions for meat (an Archlinux AUR helper).

set -l listinstalled "(pacman -Qm | tr ' ' \t)"
set -l listall       "(pacman -Sl | cut --delim ' ' --fields 2- | tr ' ' \t)"
set -l listrepos    "(__fish_print_pacman_repos)"
set -l listgroups    "(pacman -Sg | sed 's/\(.*\)/\1\tPackage group/g')"


# Operations:
complete -c meat -f -s h -l 'help'             -d 'Display help and quit'
complete -c meat -f -s d -l 'download'         -d 'Download and install targets'
complete -c meat -f -s G -l 'git-db-update'    -d 'Update the git database'
complete -c meat -f -s i -l 'info'             -d 'Show info for target [twice for more details]'
complete -c meat -f -s s -l 'search'           -d 'Search for packages by name'
complete -c meat -f -s m -l 'msearch'          -d 'Search for packages by maintainer'
complete -c meat -f -s u -l 'update'           -d 'Check for updates and install'
complete -c meat -f -s U -l 'listupdates'      -d 'Check for updates and list them'

# General options:
complete -c meat -f -s f -l 'force'            -d 'Continue no matter what [not recommended]'
complete -c meat -f -s g -l 'git-check'        -d 'Update checksums for git files'
complete -c meat -f      -l 'ignoregit' -xa "$listinstalled" -d 'Ignore PKG when checking for git updates'
complete -c meat -f      -l 'ignore' -xa "$listinstalled" -d 'Ignore PKG upgrade'
complete -c meat -f      -l 'ignorerepo' -xa "$listrepos" -d 'Ignore REPO when checking for updates'
complete -c meat -f      -l 'nossl'            -d "Don't use https connections"
complete -c meat -f      -l 'sign'             -d "GPG sign the resulting package"
complete -c meat -f      -l 'nosign'           -d "Don't GPG sign the resulting package"
complete -c meat -f -s t -l 'target'           -d 'Download targets to DIR'
complete -c meat -f      -l 'threads'          -d 'Limit the number of threads created [10]'
complete -c meat -f      -l 'timeout'          -d 'Curl timeout in seconds'
complete -c meat -f      -l 'check-all'        -d 'Check all files when installing'

# Output options:
complete -c meat -f -s c -l 'color' -a 'always auto never' -d 'Use colored output'
complete -c meat -f      -l 'debug'            -d 'Show debug output'
complete -c meat -f      -l 'format'           -d 'Print formatted'
complete -c meat -f -s q -l 'quiet'            -d 'Output less'
complete -c meat -f -s v -l 'verbose'          -d 'Output more'

# Complete with AUR packages:
complete -c meat -f --condition 'not expr (commandline --current-token) : "^-.*" > /dev/null' --arguments '(cower --format="%n\n" --search (commandline --current-token))'
