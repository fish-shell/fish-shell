# Completions for xbps-rindex
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-rindex
set -l modes none gzip bzip2 xz lz4 zstd

complete -c $progname -f

complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -l compression -d 'Set the repodata compression format. If unset, defaults to zstd.' -xa "$modes"
complete -c $progname -s C -d 'Check not only for file existence but for the correct file hash while cleaning.'
complete -c $progname -s f -d 'Forcefully register binary package into the local repository, overwriting existing entry.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s V -d 'Show the version information.'
complete -c $progname -l signedby -d 'Name and email of person signing the repository.'
complete -c $progname -l privkey -d 'Path to the private RSA key to sign the repository. Defaults to ~/.ssh/id_rsa' -F

complete -c $progname -s a -d 'Registers the binary package into the local repository.' -xa "(__fish_complete_suffix .xbps)"
complete -c $progname -s c -d 'Removes obsolete entries found in the local repository.' -xa "(__fish_complete_directories)"
complete -c $progname -s r -d 'Removes obsolete and currently unregistered packages from the local repository.' -xa "(__fish_complete_directories)"
complete -c $progname -s s -d 'Initializes a signed repository with your specified RSA key.' -xa "(__fish_complete_directories)"
complete -c $progname -s S -d 'Signs a binary package archive with your specified RSA key.' -xa "(__fish_complete_suffix .xbps)"
