# Completions for xbps-digest
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-digest
set -l modes sha256

complete -c $progname -s m -d 'Sets the message digest mode. If unset, defaults to sha256' -xa "$modes"
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s V -d 'Show the version information.'
