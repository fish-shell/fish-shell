complete -c nmcli -s t -l terse   -d 'Output is terse'
complete -c nmcli -s p -l pretty  -d 'Output is pretty'
complete -c nmcli -s m -l mode    -xa 'tabular multiline' -d 'Switch between tabular and multiline mode'
complete -c nmcli -s f -l fields  -xa 'all common' -d 'Specify the output fields'
complete -c nmcli -s e -l escape  -xa 'yes no' -d 'Whether to escape ":" and "\\" characters'
complete -c nmcli -s v -l version -d 'Show nmcli version'
complete -c nmcli -s h -l help    -d 'Print help information'
complete -c nmcli -d 'Command-line tool to control NetworkManager' -x

complete -c nmcli -xa nm -n '__fish_use_subcommand' -d 'Inquiry and change the state of NM' -x
complete -c nmcli -n '__fish_seen_subcommand_from nm; and not __fish_seen_subcommand_from status enable sleep wifi wwan' -xa 'status enable sleep wifi wwan'
complete -c nmcli -n '__fish_seen_subcommand_from nm; and __fish_seen_subcommand_from enable sleep' -xa 'true false'
complete -c nmcli -n '__fish_seen_subcommand_from nm; and __fish_seen_subcommand_from wifi wwan' -xa 'on off'

complete -c nmcli -xa con -n '__fish_use_subcommand' -d "Get information about NM's connections" -x
complete -c nmcli -n '__fish_seen_subcommand_from con; and not __fish_seen_subcommand_from list status up down delete' -xa 'list status up down delete'
complete -c nmcli -n '__fish_seen_subcommand_from con; and __fish_seen_subcommand_from list up down delete' -xa 'id uuid'
complete -c nmcli -n 'contains_seq con up -- (commandline -op)' -xa 'iface ap'
complete -c nmcli -n 'contains_seq con up iface -- (commandline -op)' -xa '(__fish_print_interfaces)'
complete -c nmcli -n 'contains_seq con up -- (commandline -op)' -l nowait -d 'Exit immediately'
complete -c nmcli -n 'contains_seq con up -- (commandline -op)' -l timeout -d 'How long to wait for operation completion' -x

complete -c nmcli -xa dev -n '__fish_use_subcommand' -d 'Get information about devices' -x
complete -c nmcli -n '__fish_seen_subcommand_from dev; and not __fish_seen_subcommand_from status list disconnect wifi' -xa 'status list disconnect wifi'
complete -c nmcli -n 'contains_seq dev disconnect -- (commandline -op)' -l nowait -d 'Exit immediately'
complete -c nmcli -n 'contains_seq dev list -- (commandline -op)' -xa 'iface'
complete -c nmcli -n '__fish_seen_subcommand_from dev; and __fish_seen_subcommand_from iface' -xa '(__fish_print_interfaces)'
complete -c nmcli -n 'contains_seq dev wifi -- (commandline -op)' -xa 'list iface'
complete -c nmcli -n 'contains_seq dev wifi list -- (commandline -op)' -xa 'essid bssid'


