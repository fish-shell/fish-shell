# Completions for userdbctl (a part of systemd)

set -l commands_users user groups-of-user
set -l commands_groups group users-in-group
set -l commands $commands_users $commands_groups
set -l services "(userdbctl services | string match -r -- '\S+' | string match -v -- 'SERVICE')"

# Commands
complete -c userdbctl -f
complete -c userdbctl -n "not __fish_seen_subcommand_from $commands" -a user -d 'Inspect user'
complete -c userdbctl -n "not __fish_seen_subcommand_from $commands" -a group -d 'Inspect group'
complete -c userdbctl -n "not __fish_seen_subcommand_from $commands" -a users-in-group -d 'Show users that are members of specified group(s)'
complete -c userdbctl -n "not __fish_seen_subcommand_from $commands" -a groups-of-user -d 'Show groups the specified user(s) is a member of'
complete -c userdbctl -n "not __fish_seen_subcommand_from $commands" -a services -d 'Show enabled database services'

complete -c userdbctl -n "__fish_seen_subcommand_from $commands_users" -a '(__fish_complete_users)'
complete -c userdbctl -n "__fish_seen_subcommand_from $commands_groups" -a '(__fish_complete_groups)'

# Options
complete -c userdbctl -s h -l help -d 'Show help'
complete -c userdbctl -l version -d 'Show version'
complete -c userdbctl -l no-pager -d 'Do not pipe output into a pager'
complete -c userdbctl -l no-legend -d 'Do not show the headers and footers'
complete -c userdbctl -l output -d 'Select output mode' -xa 'classic friendly table json'
complete -c userdbctl -s j -d 'Equivalent to --output=json'
complete -c userdbctl -s s -l service -d 'Query the specified service' -xa "$services"
complete -c userdbctl -l with-nss -d 'Control whether to include glibc NSS data' -xa 'yes no'
complete -c userdbctl -s N -d 'Do not synthesize or include glibc NSS data'
complete -c userdbctl -l synthesize -d 'Synthesize root/nobody user' -xa 'yes no'
