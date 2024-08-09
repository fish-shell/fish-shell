# Completions for xbps-install
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-install

set -l listall "(__fish_print_xbps_packages)"

complete -c $progname -f
complete -c $progname -a "$listall"

complete -c $progname -s A -d 'Install as a dependency'
complete -c $progname -s C -d 'Use this XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s c -d 'Use this cache directory to store binary packages' -xa "(__fish_complete_directories)"
complete -c $progname -s d -d 'Enable extra debugging shown to stderr'
complete -c $progname -s D -d 'Only download packages to the cache'
complete -c $progname -s f -d 'Force downgrade/reinstall package (pass twice to reinstall config files as well)'
complete -c $progname -s h -d 'Show the help message'
complete -c $progname -s I -d 'Ignore detected file conflicts in a transaction'
complete -c $progname -s i -d 'Ignore repositories defined in configuration files'
complete -c $progname -s M -d 'For remote repositories, the data is fetched and stored in memory'
complete -c $progname -s n -d 'Dry-run mode.  Show what actions would be done but don\'t do anything'
complete -c $progname -s R -d 'Enable repository mode' -xa "(__fish_complete_directories)"
complete -c $progname -l repository -d 'Append the specified repository to the top of the list' -xa "(__fish_complete_directories)"
complete -c $progname -s r -d 'Use this target root directory' -xa "(__fish_complete_directories)"
complete -c $progname -s S -d 'Synchronize remote repository index files'
complete -c $progname -s U -d 'Don\'t configure packages'
complete -c $progname -s u -d 'Performs a full system upgrade (except for packages on hold)'
complete -c $progname -s v -d 'Enables verbose messages'
complete -c $progname -s y -d 'Assume yes to all questions and avoid interactive questions'
complete -c $progname -s V -d 'Show the version information'
