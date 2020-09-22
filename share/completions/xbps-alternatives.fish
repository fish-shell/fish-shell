# xbps-alternatives
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-alternatives

set -l listinstalled "(__fish_print_xbps_packages -i)"

complete -c $progname -f
complete -c $progname -a "$listinstalled"

complete -c $progname -s C -d 'Specifies a path to the XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s g -d 'Alternative group name to match.  To be used with the set mode.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s r -d 'Specifies a full path for the target root directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s v -d 'Enables verbose messages.'
complete -c $progname -s V -d 'Show the version information.'
complete -c $progname -s l -n "not __fish_contains_opt -s s" -d 'Lists all current alternative groups or only from PKG, or just a specific group with -g.'
complete -c $progname -s s -n "not __fish_contains_opt -s l" -d 'Set alternative groups specified by PKG or just a specific group with -g.'
