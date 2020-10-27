set -l commands list info dump debug

complete -c coredumpctl -f
complete -c coredumpctl -n "not __fish_seen_subcommand_from $commands" -d 'List available coredumps'
complete -c coredumpctl -n "not __fish_seen_subcommand_from $commands" -d 'Show detailed information about coredump(s)'
complete -c coredumpctl -n "not __fish_seen_subcommand_from $commands" -d 'Print first matching coredump to stdout'
complete -c coredumpctl -n "not __fish_seen_subcommand_from $commands" -d 'Start a debugger for the first matching coredump'

complete -c coredumpctl -s h -l help -d 'Show this help'
complete -c coredumpctl -l version -d 'Print version string'
complete -c coredumpctl -l no-pager -d 'Do not pipe output into a pager'
complete -c coredumpctl -l no-legend -d 'Do not print the column headers'
complete -c coredumpctl -l debugger -r -d 'Use the given DEBUGGER'
complete -c coredumpctl -s 1 -d 'Show information about most recent entry only'
complete -c coredumpctl -s S -l since -r -d 'Only print coredumps since the DATE'
complete -c coredumpctl -s U -l until -r -d 'Only print coredumps until the DATE'
complete -c coredumpctl -s r -l reverse -d 'Show the newest entries first'
complete -c coredumpctl -s F -l field -r -d 'List all values a certain FIELD takes'
complete -c coredumpctl -s o -l output -r -d 'Write output to FILE'
complete -c coredumpctl -l file -r -d 'Use journal FILE'
complete -c coredumpctl -s D -l directory -r -d 'Use journal files from DIRECTORY'
complete -c coredumpctl -s q -l quiet -d 'Do not show info messages and privilege warning'
