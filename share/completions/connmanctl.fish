function __fish_print_connman_services
    # connmanctl services follows this pattern (to be interpreted as a regex):
    # "[\* ][A ][ORacd ] service_name +identification_string"
    # where [\* ] indicates whether a service is a favorite
    # [A ] indicates whether a service is in autoconnect mode
    # [ORacd ] indicates the current status of the service: online, ready, association, configuration, disconnect or idle
    connmanctl services | string replace -r '.{4}(.*) +(.*)' '$2\t$1'
end

function __fish_print_connman_technologies
    connmanctl technologies | string match '*Type*' | string replace -r '  Type = (.*)' '$1'
end

function __fish_print_connman_vpnconnections
    # formatting of the vpn connections:
    # "  [RCF ] service_name +identification_string"
    # where [RCF ] indicates the current status which can be: ready, configuration, failure or idle
    connmanctl vpnconnections | string replace -r '.{4}(.*) +(.*)' '$2\t$1'
end

# connmanctl does not accept options before commands, so requiring the commands to be in second position is okay
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a state -d "Shows if the system is online or offline"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a technologies -d "Display technologies"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a clock -d "Get System Clock Properties"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a enable -d "Enables given technology or offline mode"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] enable" -a "(__fish_print_connman_technologies) offline"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a disable -d "Disables given technology or offline mode"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] disable" -a "(__fish_print_connman_technologies) offline"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a tether -d "Enable, disable tethering, set SSID and passphrase for wifi"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] tether" -a "(__fish_print_connman_technologies)"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 3; and contains -- (commandline -xpc)[2] tether" -a "on off"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a services -d "Display services"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] services" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a peers -d "Display peers"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a scan -d "Scans for new services for given technology"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] scan" -a "(__fish_print_connman_technologies)"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a connect -d "Connect a given service or peer"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] connect" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a disconnect -d "Disconnect a given service or peer"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] disconnect" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a config -d "Set service configuration options"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -ge 2; and contains -- (commandline -xpc)[2] config" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a monitor -d "Monitor signals from interfaces"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a vpnconnections -d "Display VPN connections"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] vpnconnections" -a "(__fish_print_connman_vpnconnections)"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a session -d "Enable or disable a session"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -eq 2; and contains -- (commandline -xpc)[2] session" -a "on off connect disconnect config"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a peer_service -d "(Un)Register a Peer Service"
complete -f -c connmanctl -n "test (count (commandline -xpc)) -lt 2" -a help -d "Show help"
