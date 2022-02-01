set -l valid_subcmds up down strip save

function __wg_complete_interfaces
    wg show 2>| string replace -rf 'Unable to access interface (.*): Operation not permitted' '$1'
end

function __wg_subcmd --no-scope-shadowing
    set -l subcmd $argv[1]
    set -e argv[1]
    complete -f -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a $subcmd $argv
end

complete -c wg-quick -f
complete -k -c wg-quick -n "__fish_seen_subcommand_from $valid_subcmds" -a '(__wg_complete_interfaces | sort)'
__wg_subcmd up -d 'Add and set up an interface'
__wg_subcmd down -d 'Tear down and remove an interface' 
__wg_subcmd strip -d 'Output config for use with wg'
__wg_subcmd save -d 'Saves the configuration of an existing interface'
