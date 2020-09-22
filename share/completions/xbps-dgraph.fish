# Completions for xbps-dgraph
# Author: Allen Sobot <chilledfrogs@disroot.org>

set -l progname xbps-dgraph

complete -c $progname -f
complete -c $progname -a "(__fish_print_xbps_packages -i)"
complete -c $progname -n "__fish_contains_opt -s R" -a "(__fish_print_xbps_packages)"

complete -c $progname -s C -d 'Specifies a path to the XBPS configuration directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s c -d 'Specifies a path to the graph configuration file.' -F
complete -c $progname -s d -d 'Enables extra debugging shown to stderr.'
complete -c $progname -s h -d 'Show the help message.'
complete -c $progname -s M -d 'For remote repositories, the data is fetched and stored in memory.'
complete -c $progname -s R -d 'Enable repository mode.'
complete -c $progname -s r -d 'Specifies a full path for the target root directory.' -xa "(__fish_complete_directories)"
complete -c $progname -s V -d 'Show the version information.'
complete -c $progname -s g -n "not __fish_contains_opt -s f" -n "not __fish_contains_opt -s m" -d 'Generates a graph configuration file in the current working directory.'
complete -c $progname -s f -n "not __fish_contains_opt -s m" -n "not __fish_contains_opt -s g" -d 'Generates a full dependency graph of the target package.'
complete -c $progname -s m -n "not __fish_contains_opt -s f" -n "not __fish_contains_opt -s g" -d 'Generates a metadata graph of the target package.' # TODO: kinda fix this, can't exactly figure out how to make these last 3 modes mutually exclusive
