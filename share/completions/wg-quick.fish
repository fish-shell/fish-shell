set -l valid_subcmds up down strip save

function __fish_wg_complete_interfaces
    LC_ALL=C wg show 2>| string replace -rf 'Unable to access interface (.*): Operation not permitted' '$1'
end

complete -c wg-quick -f
complete -c wg-quick -n "__fish_seen_subcommand_from $valid_subcmds" -a '(__fish_wg_complete_interfaces)'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a up -d 'Add and set up an interface'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a down -d 'Tear down and remove an interface'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a strip -d 'Output config for use with wg'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a save -d 'Saves the configuration of an existing interface'
