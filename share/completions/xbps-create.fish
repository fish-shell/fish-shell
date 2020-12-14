# Completions for xbps-create
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-create
set -l modes none gzip bzip2 xz lz4 zstd

complete -c $progname -f -a "(__fish_complete_directories)"

complete -c $progname -s A -d 'The target architecture for this package (required)' -x
complete -c $progname -s B -d 'The package builder string, free form' -x
complete -c $progname -s C -d 'Package patterns this package conflicts with, separated by whitespaces' -x
complete -c $progname -s D -d 'Package patterns this package depends on, separated by whitespaces' -x
complete -c $progname -s F -d 'Configuration files this package provides, separated by whitespace' -x
complete -c $progname -s G -d 'Set git revisions of the sourcepkg used to build binary package' -x
complete -c $progname -s H -d 'The package homepage string' -x
complete -c $progname -s h -d 'Show the help message'
complete -c $progname -s l -d 'The package license' -x
complete -c $progname -s M -d 'A list of mutable files this package provides, separated by whitespaces' -x
complete -c $progname -s m -d 'The package maintainer name and/or email contact' -x
complete -c $progname -s n -d 'The package name/version tuple, e. g: \'foo-1. 0_1\'' -x
complete -c $progname -s P -d 'Virtual packages this package provides, separated by whitespace' -x
complete -c $progname -s p -d 'Preserve package files after being updated'
complete -c $progname -s q -d 'Enable quiet operation'
complete -c $progname -s R -d 'Package patterns this package replaces, separated by whitespaces' -x
complete -c $progname -s r -d 'Versions this package reverts, separated by whitespaces' -x
complete -c $progname -s S -d 'A long description for this package' -x
complete -c $progname -s s -d 'A short description for this package, one line under 80 characters' -x
complete -c $progname -s t -d 'Tags (categories) for this package, separated by whitespace' -x
complete -c $progname -s V -d 'Show the version information'
complete -c $progname -l build-options -d 'A string containing the build options used in package' -x
complete -c $progname -l compression -d 'Set the binary package compression format (default: zstd)' -xa "$modes"
complete -c $progname -l shlib-provides -d 'Provided shared libraries, separated by whitespace' -x
complete -c $progname -l shlib-requires -d 'Required shared libraries, separated by whitespace' -x
complete -c $progname -l alternatives -d 'Alternatives provided by this package, separated by whitespace' -x
complete -c $progname -s c -d 'The package changelog string' -x
