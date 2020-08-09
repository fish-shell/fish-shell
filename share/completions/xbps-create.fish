# Completions for xbps-create
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-create
set -l modes none gzip bzip2 xz lz4 zstd

complete -c $progname -f -a "(__fish_complete_directories)"

complete -c $progname -s A -d 'The target architecture for this package (required).' -x
complete -c $progname -s B -d 'The package builder string, free form.' -x
complete -c $progname -s C -d 'A list of package patterns that this package should conflict with, separated by whitespaces.' -x
complete -c $progname -s D -d 'A list of package patterns this package depends on, separated by whitespaces.' -x
complete -c $progname -s F -d 'A list of configuration files this package provides, separated by whitespace.' -x
complete -c $progname -s G -d 'This sets a string with the git revisions of the sourcepkg that was used to binary package.' -x
complete -c $progname -s H -d 'The package homepage string.' -x
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s l -d 'The package license.' -x
complete -c $progname -s M -d 'A list of mutable files this package provides, separated by whitespaces.' -x
complete -c $progname -s m -d 'The package maintainer name and/or email contact.' -x
complete -c $progname -s n -d 'The package name/version tuple, e. g: \'foo-1. 0_1\'.' -x
complete -c $progname -s P -d 'A list of virtual packages this package provides, separated by whitespaces.' -x
complete -c $progname -s p -d 'If set the package files will be preserved after being updated.'
complete -c $progname -s q -d 'Enable quiet operation.'
complete -c $progname -s R -d 'A list of package patterns this package replaces, separated by whitespaces.' -x
complete -c $progname -s r -d 'A list of versions this package reverts, separated by whitespaces.' -x
complete -c $progname -s S -d 'A long description for this package.' -x
complete -c $progname -s s -d 'A short description for this package, one line with less than 80 characters.' -x
complete -c $progname -s t -d 'A list of tags (categories) this package should be part of, separated by whit…' -x
complete -c $progname -s V -d 'Show the version information.'
complete -c $progname -l build-options -d 'A string containing the build options used in package.' -x
complete -c $progname -l compression -d 'Set the binary package compression format. If unset, defaults to zstd.' -xa "$modes"
complete -c $progname -l shlib-provides -d 'A list of provided shared libraries, separated by whitespaces.' -x
complete -c $progname -l shlib-requires -d 'A list of required shared libraries, separated by whitespaces.' -x
complete -c $progname -l alternatives -d 'A list of alternatives provided by this package, separated by whitespaces.' -x
complete -c $progname -s c -d 'The package changelog string.' -x
