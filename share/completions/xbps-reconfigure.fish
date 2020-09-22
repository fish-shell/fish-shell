# Completions for xbps-reconfigure
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-reconfigure

set -l listinstalled "(__fish_print_xbps_packages -i)"

complete -c $progname -f
complete -c $progname -a "$listinstalled"

complete -c $progname -s a -d 'Configures all packages.'
complete -c $progname -s C -d 'Specifies a path to the XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s f -d 'Forcefully reconfigure package even if it was configured previously.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s i -n "__fish_contains_opt -s a" -d 'Ignore PKG when configuring all packages, can be specified multiple times.' -xa "$listinstalled"
complete -c $progname -s r -d 'Specifies a path for the target root directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s v -d 'Enables verbose messages.'
complete -c $progname -s V -d 'Show the version information.'
