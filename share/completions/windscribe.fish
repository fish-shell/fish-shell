#Windscribe 1.4

#variables
set -l seen __fish_seen_subcommand_from
set -l commands account connect disconnect examples firewall lanbypass locations login logout port protocol proxy sendlog speedtest status viewlog

#options
complete -c windscribe -x -n "not $seen $commands" -l help

#commands
complete -c windscribe -f -n "$seen $commands"
complete -c windscribe -f -n "not $seen $commands" -a account -d "Output current account details"
complete -c windscribe -f -n "not $seen $commands" -a connect -d "Connect to Windscribe"
complete -c windscribe -f -n "not $seen $commands" -a disconnect -d "Disconnect from VPN"
complete -c windscribe -f -n "not $seen $commands" -a examples -d "Show usage examples"
complete -c windscribe -f -n "not $seen $commands" -a firewall -d "View/Modify Firewall mode"
complete -c windscribe -f -n "not $seen $commands" -a lanbypass -d "View/Modify Firewall LAN bypass"
complete -c windscribe -f -n "not $seen $commands" -a locations -d "Output list of all available server locations"
complete -c windscribe -f -n "not $seen $commands" -a login -d "Login to Windscribe account"
complete -c windscribe -f -n "not $seen $commands" -a logout -d "Logout and disconnect"
complete -c windscribe -f -n "not $seen $commands" -a port -d "View/Modify default Port"
complete -c windscribe -f -n "not $seen $commands" -a protocol -d "View/Modify default Protocol"
complete -c windscribe -f -n "not $seen $commands" -a proxy -d "View/Modify Proxy Settings"
complete -c windscribe -f -n "not $seen $commands" -a sendlog -d "Send the debug log to Support"
complete -c windscribe -f -n "not $seen $commands" -a speedtest -d "Test the connection speed"
complete -c windscribe -f -n "not $seen $commands" -a status -d "Check status of Windscribe and connection"
complete -c windscribe -f -n "not $seen $commands" -a viewlog -d "View the debug log"
