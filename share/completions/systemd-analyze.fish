complete -c systemd-analyze -x
complete -f -c systemd-analzye -s H -l host -d 'Execute the operation on a remote host' -a "(__fish_print_hostnames)"
complete -x -c systemd-analzye -s M -l machine -d 'Execute operation on a VM or container' -a "(__fish_systemd_machines)"
complete -f -c systemd-analzye -s h -l help -d 'Print a short help and exit'
complete -f -c systemd-analzye -l version -d 'Print a short version and exit'
complete -f -c systemd-analzye -l no-pager -d 'Do not pipe output into a pager'
complete -f -c systemd-analzye -l user -d 'Talk to the service manager of the calling user' -n "not __fish_contains_opt system user"
complete -f -c systemd-analzye -l system -d 'Talk to the service manager of the system.' -n "not __fish_contains_opt system user"

set -l commands time blame critical-chain plot dot set-log-level set-log-target dump verify

complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a time -d  "Print time spent in the kernel" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a blame -d  "Print list of running units ordered by time to init" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a critical-chain -d  "Print a tree of the time critical chain of units" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a plot -d  "Output SVG graphic showing service initialization" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a dot -d  "Output dependency graph in dot(1) format" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a set-log-level -d  "Set logging threshold for manager" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a set-log-target -d  "Set logging target for manager" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a dump -d  "Output state serialization of service manager" -f
complete -c systemd-analyze -n "not __fish_seen_subcommand_from $commands" -a verify -d  "Check unit files for correctness"
