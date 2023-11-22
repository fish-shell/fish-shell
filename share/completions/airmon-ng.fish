#airmon-ng

set -l commands "start stop check"
complete -c airmon-ng -x -n "not __fish_seen_subcommand_from $commands" -a "$commands"
complete -c airmon-ng -x -n "__fish_seen_subcommand_from $commands; and not __fish_seen_subcommand_from (__fish_print_interfaces)" -a "(__fish_print_interfaces)"
complete -c airmon-ng -f
