set -l nmoutput (nmcli -g NAME connection show --active 2>/dev/null)
or exit # networkmanager isn't running, no point in completing
set -l cname (string escape -- $nmoutput\t"Active connection")
set -a cname (string escape -- (nmcli -g NAME connection show)\t"Connection")
set -l ifname (string escape -- (nmcli -g DEVICE device status)\t"Interface name")
set -l ssid (string escape -- (nmcli -g SSID device wifi list)\t"SSID")
set -l bssid (string escape -- (nmcli -g BSSID device wifi list | string replace --all \\ '')\t"BSSID")

set -l nmcli_commands general networking radio connection device agent monitor help
set -l nmcli_general status hostname permissions logging help
set -l nmcli_networking on off connectivity help
set -l nmcli_radio all wifi wwan help
set -l nmcli_connection show up down add modify clone edit delete monitor reload load import export help
set -l nmcli_device status show set connect reapply modify disconnect delete monitor wifi lldp help
set -l nmcli_agent secret polkit all help

complete -c nmcli -s t -l terse -d 'Output is terse' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s p -l pretty -d 'Output is pretty' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s m -l mode -d 'Switch between tabular and multiline mode' -xa 'tabular multiline' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s c -l color -d 'Whether to use colors in output' -xa 'auto yes no' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s f -l fields -d 'Specify the output fields' -xa 'all common' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s e -l escape -d 'Whether to escape ":" and "\\" characters' -xa 'yes no' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s a -l ask -d 'Ask for missing parameters' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s s -l show-secrets -d 'Allow displaying passwords' -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s w -l wait -d 'Set timeout (seconds) waiting for finishing operations' -x -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s v -l version -d 'Show nmcli version' -x -n "not __fish_seen_subcommand_from $nmcli_commands"
complete -c nmcli -s h -l help -d 'Print help information' -x
complete -c nmcli -d 'Command-line tool to control NetworkManager'

complete -c nmcli -n "not __fish_seen_subcommand_from $nmcli_commands" -xa "$nmcli_commands"

complete -c nmcli -n "__fish_seen_subcommand_from general; and not __fish_seen_subcommand_from $nmcli_general" -xa status -d 'Show overall status of NetworkManager'
complete -c nmcli -n "__fish_seen_subcommand_from general; and not __fish_seen_subcommand_from $nmcli_general" -xa hostname -d 'Get or change persistent system hostname'
complete -c nmcli -n "__fish_seen_subcommand_from general; and not __fish_seen_subcommand_from $nmcli_general" -xa permissions -d 'Show caller permissions for authenticated operations'
complete -c nmcli -n "__fish_seen_subcommand_from general; and not __fish_seen_subcommand_from $nmcli_general" -xa logging -d 'Get or change NetworkManager logging level and domains'
complete -c nmcli -n "__fish_seen_subcommand_from general; and not __fish_seen_subcommand_from $nmcli_general" -xa help
complete -c nmcli -n "contains_seq general logging -- (commandline -xp)" -xa 'level domains help'

complete -c nmcli -n "__fish_seen_subcommand_from networking; and not __fish_seen_subcommand_from $nmcli_networking" -xa on -d 'Switch networking on'
complete -c nmcli -n "__fish_seen_subcommand_from networking; and not __fish_seen_subcommand_from $nmcli_networking" -xa off -d 'Switch networking off'
complete -c nmcli -n "__fish_seen_subcommand_from networking; and not __fish_seen_subcommand_from $nmcli_networking" -xa connectivity -d 'Get network connectivity state'
complete -c nmcli -n "__fish_seen_subcommand_from networking; and not __fish_seen_subcommand_from $nmcli_networking" -xa help
complete -c nmcli -n "contains_seq networking connectivity -- (commandline -xp)" -xa check -d 'Re-check the connectivity'

complete -c nmcli -n "__fish_seen_subcommand_from radio; and not __fish_seen_subcommand_from $nmcli_radio" -xa all -d 'Get status of all radio switches; turn them on/off'
complete -c nmcli -n "contains_seq radio all -- (commandline -xp)" -xa 'on off help'
complete -c nmcli -n "__fish_seen_subcommand_from radio; and not __fish_seen_subcommand_from $nmcli_radio" -xa wifi -d 'Get status of Wi-Fi radio switch; turn it on/off'
complete -c nmcli -n "contains_seq radio wifi -- (commandline -xp)" -xa 'on off help'
complete -c nmcli -n "__fish_seen_subcommand_from radio; and not __fish_seen_subcommand_from $nmcli_radio" -xa wwan -d 'Get status of mobile broadband radio switch; turn it on/off'
complete -c nmcli -n "contains_seq radio wwan -- (commandline -xp)" -xa 'on off help'

complete -c nmcli -n "__fish_seen_subcommand_from connection; and not __fish_seen_subcommand_from $nmcli_connection" -xa "$nmcli_connection"
# Connection subcommands are self-explanatory, I'm just highlighting a difference between edit and modify
complete -c nmcli -n "__fish_seen_subcommand_from connection; and not __fish_seen_subcommand_from $nmcli_connection" -xa modify -d "Modify one or more properties"
complete -c nmcli -n "__fish_seen_subcommand_from connection; and not __fish_seen_subcommand_from $nmcli_connection" -xa edit -d "Interactive edit"
complete -c nmcli -n "contains_seq connection show -- (commandline -xp)" -l active -d 'List only active profiles'
complete -c nmcli -n "contains_seq connection show -- (commandline -xp)" -l order -d 'Custom connection ordering'
complete -c nmcli -n "contains_seq connection show -- (commandline -xp)" -xa "help $cname" -k
complete -c nmcli -n "contains_seq connection up -- (commandline -xp) && not contains -- (commandline -xp)[-1] ifname ap" -xa "help $cname" -k
complete -c nmcli -n "contains_seq connection up -- (commandline -xp) && test (commandline -xp)[-1] = ifname" -xa "$ifname" -k
complete -c nmcli -n "contains_seq connection up -- (commandline -xp) && test (commandline -xp)[-1] = ap" -xa "$bssid"
complete -c nmcli -n "contains_seq connection up -- (commandline -xp)" -xa nsp -d 'Specify NSP to connect to (only for WiMAX)'
complete -c nmcli -n "contains_seq connection up -- (commandline -xp)" -xa passwd-file -d 'password file to activate the connection'
complete -c nmcli -n "contains_seq connection down -- (commandline -xp)" -xa "help $cname" -k
complete -c nmcli -n "contains_seq connection add -- (commandline -xp)" -xa 'type ifname con-name autoconnect save master slave-type help'
complete -c nmcli -n "contains_seq connection add -- (commandline -xp) && test (commandline -xp)[-1] = ifname" -xa "$ifname"
complete -c nmcli -n "contains_seq connection modify -- (commandline -xp)" -l temporary
complete -c nmcli -n "contains_seq connection modify -- (commandline -xp)" -xa "help $cname" -k
complete -c nmcli -n "contains_seq connection clone -- (commandline -xp)" -l temporary
complete -c nmcli -n "contains_seq connection clone -- (commandline -xp)" -xa "help $cname" -k
complete -c nmcli -n "contains_seq connection edit -- (commandline -xp)" -xa "type help $cname" -k
complete -c nmcli -n "contains_seq connection edit type -- (commandline -xp)" -xa con-name
complete -c nmcli -n "contains_seq connection delete -- (commandline -xp)" -xa "help $cname" -k
complete -c nmcli -n "contains_seq connection monitor -- (commandline -xp)" -xa "help $cname" -k
complete -c nmcli -n "contains_seq connection import -- (commandline -xp)" -l temporary
complete -c nmcli -n "contains_seq connection import -- (commandline -xp)" -xa 'type file help'
complete -c nmcli -n "contains_seq connection export -- (commandline -xp)" -xa "help $cname" -k

set -l wifi_commands list connect hotspot rescan help
complete -c nmcli -n "__fish_seen_subcommand_from device; and not __fish_seen_subcommand_from $nmcli_device" -xa "$nmcli_device"
complete -c nmcli -n "contains_seq device set -- (commandline -xp)" -xa 'ifname autoconnect managed'
complete -c nmcli -n "contains_seq device wifi -- (commandline -xp); and not __fish_seen_subcommand_from $wifi_commands" -xa "$wifi_commands"
complete -c nmcli -n "contains_seq device wifi list -- (commandline -xp)" -xa 'ifname bssid'
complete -c nmcli -n "contains_seq device wifi list ifname -- (commandline -xp)" -xa "$ifname"
complete -c nmcli -n "contains_seq device wifi list bssid -- (commandline -xp)" -xa "$bssid"
complete -c nmcli -n "contains_seq device wifi connect -- (commandline -xp)" -xa "$ssid $bssid password wep-key-type ifname bssid name private hidden" -k
complete -c nmcli -n "contains_seq device wifi connect ifname -- (commandline -xp)" -xa "$ifname"
complete -c nmcli -n "contains_seq device wifi connect bssid -- (commandline -xp)" -xa "$bssid"
complete -c nmcli -n "contains_seq device wifi hotspot -- (commandline -xp)" -xa 'ifname con-name ssid band channel password'
complete -c nmcli -n "contains_seq device wifi hotspot ifname -- (commandline -xp)" -xa "$ifname"
complete -c nmcli -n "contains_seq device wifi rescan -- (commandline -xp)" -xa 'ifname ssid'
complete -c nmcli -n "contains_seq device wifi rescan ifname -- (commandline -xp)" -xa "$ifname"
complete -c nmcli -n "contains_seq device wifi rescan ssid -- (commandline -xp)" -xa "$ssid"
complete -c nmcli -n "contains_seq device lldp -- (commandline -xp)" -xa list

complete -c nmcli -n "__fish_seen_subcommand_from agent; and not __fish_seen_subcommand_from $nmcli_agent" -xa secret -d "Register nmcli as NM secret agent"
complete -c nmcli -n "__fish_seen_subcommand_from agent; and not __fish_seen_subcommand_from $nmcli_agent" -xa polkit -d "Register nmcli as a polkit agent for user session"
complete -c nmcli -n "__fish_seen_subcommand_from agent; and not __fish_seen_subcommand_from $nmcli_agent" -xa all -d "Run nmcli as secret and polkit agent"
