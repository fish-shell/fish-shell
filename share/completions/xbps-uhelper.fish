# Completions for xbps-uhelper
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-uhelper
set -l actions binpkgarch binpkgver cmpver getpkgdepname getpkgdepversion getpkgname getpkgrevision getpkgversion pkgmatch version real-version

complete -c $progname -f -a "$actions (__fish_print_xbps_packages -i)"

complete -c $progname -s C -d 'Path to xbps.conf file.' -rF
complete -c $progname -s d -d 'Debugging messages to stderr.'
complete -c $progname -s r -d 'Path to rootdir.' -xa "(__fish_complete_directories)"
complete -c $progname -s V -d 'Prints the XBPS release version.'
