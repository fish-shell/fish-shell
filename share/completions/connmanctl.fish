function __fish_print_connman_services
    connmanctl services | string replace -r '.*? +(.*) +(.*)' '$2\t$1'
end

function __fish_print_connman_technologies
    connmanctl technologies | string match '*Type*' | string replace -r '  Type = (.*)' '$1'
end

function __fish_print_connman_vpnconnections
    connmanctl vpnconnections | string replace -r '.* (.*)' '$1'
end

complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "state" -d "Shows if the system is online or offline"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "technologies" -d "Display technologies"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "clock" -d "Get System Clock Properties"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "enable" -d "Enables given technology or offline mode"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] enable" -a "(__fish_print_connman_technologies) offline" 
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "disable" -d "Disables given technology or offline mode"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] disable" -a "(__fish_print_connman_technologies) offline"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "tether" -d "Enable, disable tethering, set SSID and passphrase for wifi"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] tether" -a "(__fish_print_connman_technologies)"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 3; and contains -- (commandline -opc)[2] tether" -a "on off"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "services" -d "Display services"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] services" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "peers" -d "Display peers"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "scan" -d "Scans for new services for given technology"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] scan" -a "(__fish_print_connman_technologies)"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "connect" -d "Connect a given service or peer"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] connect" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "disconnect" -d "Disconnect a given service or peer"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] disconnect" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "config" -d "Set service configuration options"
complete -f -c connmanctl -n "test (count (commandline -opc)) -ge 2; and contains -- (commandline -opc)[2] config" -a "(__fish_print_connman_services)"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "monitor" -d "Monitor signals from interfaces"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "vpnconnections" -d "Display VPN connections"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] vpnconnections" -a "(__fish_print_connman_vpnconnections)"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "session" -d "Enable or disable a session"
complete -f -c connmanctl -n "test (count (commandline -opc)) -eq 2; and contains -- (commandline -opc)[2] session" -a "on off connect disconnect config"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "peer_service" -d "(Un)Register a Peer Service"
complete -f -c connmanctl -n "test (count (commandline -opc)) -lt 2" -a "help" -d "Show help"
