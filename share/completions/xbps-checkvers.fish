# Completions for xbps-checkvers
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-checkvers

complete -c $progname -s C -d 'Specifies a path to the XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s D -d 'Specifies a full path to the void-packages repository.' -xa "(__fish_complete_directories)"
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s f -d 'Format according to the string format, inspired by printf.' -x
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s i -d 'Ignore repositories defined in configuration files.'
complete -c $progname -s I -d 'Check for outdated installed packages rather than in repositories.'
complete -c $progname -s m -d 'Only process listed files.'
complete -c $progname -s R -d 'Repository to be added to the top of the list.'
complete -c $progname -s r -d 'Specifies a full path for the target root directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s s -d 'List all packages found in the void-packages tree and prints available version.'
complete -c $progname -s V -d 'Show the version information.'
