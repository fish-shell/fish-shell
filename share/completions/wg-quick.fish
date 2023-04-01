set -l valid_subcmds up down strip save

function __ghoti_wg_complete_interfaces
    wg show interfaces
end

complete -c wg-quick -f
complete -c wg-quick -n "__ghoti_seen_subcommand_from $valid_subcmds" -a '(__ghoti_wg_complete_interfaces)'
complete -c wg-quick -n "not __ghoti_seen_subcommand_from $valid_subcmds" -a up -d 'Add and set up an interface'
complete -c wg-quick -n "not __ghoti_seen_subcommand_from $valid_subcmds" -a down -d 'Tear down and remove an interface'
complete -c wg-quick -n "not __ghoti_seen_subcommand_from $valid_subcmds" -a strip -d 'Output config for use with wg'
complete -c wg-quick -n "not __ghoti_seen_subcommand_from $valid_subcmds" -a save -d 'Saves the configuration of an existing interface'
