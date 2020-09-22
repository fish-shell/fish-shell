# Completions for xbps-pkgdb
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-pkgdb
set -l modes auto manual hold unhold repolock repounlock

complete -c $progname -f

complete -c $progname -s a -d 'Process all registered packages, regardless of its state.'
complete -c $progname -s C -d 'Specifies a path to the XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s m -d 'Set mode of PKGNAME' -xa "$modes"
complete -c $progname -n "__fish_seen_subcommand_from $modes" -xa "(__fish_print_xbps_packages -i)"
complete -c $progname -s r -d 'Specifies a full path for the target root directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s u -d 'Updates the pkgdb format to the latest version making the necessary conversions.'
complete -c $progname -s v -d 'Enables verbose messages.'
complete -c $progname -s V -d 'Show the version information.'
