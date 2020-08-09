# Completions for xbps-fetch
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-fetch

complete -c $progname -f

complete -c $progname -s d -d 'Enables debug messages on stderr.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s o -d 'Rename file from specified URL to output.' -F
complete -c $progname -s v -d 'Enables verbose messages.'
complete -c $progname -s V -d 'Show the version information.'
