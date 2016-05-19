set -l cmds status list lldp

complete -c networkctl -f -n '__fish_seen_subcommand_from status' -a '(networkctl list --no-pager --no-legend -a | string trim \
| string replace -r \'([0-9]+) (\w+) .*$\' \'$2\t$1\n$1\t$2\')'
complete -c networkctl -x -n "not __fish_seen_subcommand_from $cmds" -a "$cmds"
