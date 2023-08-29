# Based on https://github.com/gnebbia/kb#usage

set -l commands add edit list view grep update delete template import export erase sync help

complete -c kb -s h -l help -d 'Show help and exit'
complete -c kb -l version -d 'Show version and exit'

complete -c kb -n "not __fish_seen_subcommand_from $commands" -a "$commands"
