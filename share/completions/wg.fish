set -l commands show showconf set setconf addconf syncconf genkey genpsk pubkey

function __fish_wg_print_interfaces
    wg show interfaces | string split ' '
end

# disable file completions
complete -c wg -f

# subcommands
complete -c wg -n "not __fish_seen_subcommand_from $commands" -x -a "$commands"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a show -d "Shows the current configuration and device information"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a showconf -d "Shows the current configuration of an interface"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a set -d "Change the current configuration"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a setconf -d "Applies a configuration to an interface"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a addconf -d "Appends a configuration to an interface"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a syncconf -d "Synchronizes a configuration to an interface"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a genkey -d "Generates a new private key"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a genpsk -d "Generates a new preshared key"
complete -c wg -n "not __fish_seen_subcommand_from $commands" -a pubkey -d "Computes public key given a private key"

complete -c wg -n "__fish_seen_subcommand_from $commands" -x -l help -d 'View usage of the subcommand'

complete -c wg -n "__fish_seen_subcommand_from addconf syncconf setconf; and __fish_is_nth_token 2" -f -a "(__fish_wg_print_interfaces)" -d 'Wireguard Interface'
complete -c wg -n "__fish_seen_subcommand_from addconf syncconf setconf; and __fish_is_nth_token 3" -F -d "Configuration file"

complete -c wg -n "__fish_seen_subcommand_from showconf; and __fish_is_nth_token 2" -f -a "(__fish_wg_print_interfaces)" -d 'Wireguard Interface'

complete -c wg -n "__fish_seen_subcommand_from show; and __fish_is_nth_token 2" -x -a interfaces -d "Shows a list of interfaces"
complete -c wg -n "__fish_seen_subcommand_from show; and __fish_is_nth_token 2" -f -a all -d "Shows information for all interfaces"
complete -c wg -n "__fish_seen_subcommand_from show; and __fish_is_nth_token 2" -f -a "(__fish_wg_print_interfaces)" -d 'Wireguard Interface'
complete -c wg -n "__fish_seen_subcommand_from show; and __fish_is_nth_token 3" -f -a "public-key private-key listen-port fwmark peers preshared-keys endpoints allowed-ips latest-handshakes transfer persistent-keepalive dump"

set -l settables fwmark private peer peer endpoint persistent allowed
set -l settables_kw $settables[..] remove preshared-key
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_is_nth_token 2" -f -a "(__fish_wg_print_interfaces)" -d 'Wireguard Interface'
complete -c wg -n "__fish_seen_subcommand_from set; and not __fish_prev_arg_in $settables_kw; and not __fish_is_nth_token 2" -f -a "$settables"
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in fwmark" -F
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in private-key" -F
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in peer" -a 'remove preshared key'
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in preshared-key" -F
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in remove" -F
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in endpoint"
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in persistent-keepalive"
complete -c wg -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in allowed-ips"
