# Completions for xbps-fbulk
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-fbulk

complete -c $progname -f -a "(__fish_complete_directories) (__fish_print_xbps_packages)"

complete -c $progname -s a -d 'Set a different target architecture, useful for cross compiling.' -x
complete -c $progname -s j -d 'Set number of parallel builds running at the same time.  By default set to 1.' -x
complete -c $progname -s l -d 'Set the log directory.  By default set to `log. <pid>`.' -x
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s v -d 'Enables verbose messages.'
complete -c $progname -s V -d 'Show the version information.'
