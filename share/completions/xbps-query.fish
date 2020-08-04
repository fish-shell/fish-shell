# Completions for xbps-query
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-query

set -l listall "(__fish_print_packages)"
set -l listinstalled "(__fish_print_packages -i)"


complete -c $progname -f
complete -c $progname -a "$listinstalled"
complete -c $progname -n "__fish_contains_opt -s R" -a "$listall"

complete -c $progname -s C -d 'Specifies a path to the XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s c -d 'Specifies a path to the cache directory, where binary packages are stored.' -xa "(__fish_complete_directories)"
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s i -d 'Ignore repositories defined in configuration files.'
complete -c $progname -s M -d 'For remote repositories, the data is fetched and stored in memory only.'
complete -c $progname -s p -d 'Only match this package property.'
complete -c $progname -s R -d 'Enable repository mode.'
complete -c $progname -l repository -d 'Appends the specified repository to the top of the list.'
complete -c $progname -l regex -d 'Enables string matching by using Extended Regular Expressions in compatible modes'
complete -c $progname -l fulldeptree -d 'Prints a full dependency tree in the show dependencies mode.'
complete -c $progname -s r -d 'Specifies a full path for the target root directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s v -d 'Enables verbose messages.'
complete -c $progname -s V -d 'Show the version information.'
complete -c $progname -s l -d 'Lists registered packages in the package database (pkgdb).'
complete -c $progname -s H -d 'List registered packages in the package database (pkgdb) that are on hold.'
complete -c $progname -s L -d 'Lists repositories and the number of packages contained on them.'
complete -c $progname -s m -d 'Lists registered packages in the package database (pkgdb) that were installed manually'
complete -c $progname -s O -d 'Lists package orphans in the package database (pkgdb)'
complete -c $progname -s o -d 'Search for installed package files by matching PATTERN.'
complete -c $progname -s S -d 'Shows information of an installed package.' -x
complete -c $progname -s s -d 'Search for packages by matching PATTERN on its pkgver and/or short_desc properties'
complete -c $progname -s f -d 'Show the package files for PKG.' -x
complete -c $progname -s x -d 'Show the required dependencies for PKG.  Only direct dependencies are shown.' -x
complete -c $progname -s X -d 'Show the reverse dependencies for PKG.' -x
complete -c $progname -l cat -d 'Prints the file FILE stored in binary package PKG to stdout.' -F
