# Completions for xbps-remove
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-remove

set -l listinstalled "(__fish_print_xbps_packages -i)"

complete -c $progname -f
complete -c $progname -a "$listinstalled"

complete -c $progname -s C -d 'Specifies a path to the XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s c -d 'Specifies a path to the cache directory, where binary packages are stored.' -xa "(__fish_complete_directories)"
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s F -d 'Forcefully remove package even if there are reverse dependencies and/or broke…'
complete -c $progname -s f -d 'Forcefully remove package files even if they have been modified.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s n -d 'Dry-run mode.  Show what actions would be done but don\'t do anything.'
complete -c $progname -s O -d 'Cleans cache directory removing obsolete binary packages.'
complete -c $progname -s o -d 'Remove orphaned packages that were installed as dependencies'
complete -c $progname -s R -d 'Recursively remove packages installed by PKG not by any other package'
complete -c $progname -s r -d 'Specifies a full path for the target root directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s v -d 'Enables verbose messages.'
complete -c $progname -s y -d 'Assume yes to all questions and avoid interactive questions.'
complete -c $progname -s V -d 'Show the version information.'
