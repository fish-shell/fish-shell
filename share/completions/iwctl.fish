set adhoc '(iwctl ad-hoc list | tail -n -2 | awk \'{print $1}\')'
set ap '(iwctl ap list | tail -n -2 | awk \'{print $1}\')'
set adpaters '(iwctl adpater list | tail -n -2 | awk \'{print $1}\')'
set devices '(iwctl device list | tail -n -2 | awk \'{print $1}\')'
set dpp '(iwctl dpp list | tail -n -2 | awk \'{print $1}\')'
set knownnetworks '(iwctl known-networks list | tail -n -2 | awk \'{print $1}\')'
set stations '(iwctl station list | tail -n -2 | awk \'{print $1}\')'
set wsc '(iwctl wsc list | tail -n -2 | awk \'{print $1}\')'
set net (iwctl station wlan0 get-networks | tail -n +5 | string replace -ra '\e\[[\d;]+m' '' | string replace -rf '^[\s>]*(\S+)\s*.*' '$1')

complete -f iwctl

# First argument
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
   -a "ad-hoc adapter ap device dpp help known-networks station version wsc"

# Second argument
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from adapter" \
    -xa "list $adapters"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from ap" \
    -xa "list $ap"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from dpp" \
    -xa "list $dpp"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from ad-hoc" \
    -xa "list $adhoc"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from device" \
    -xa "list $devices"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from known-networks" \
    -xa "list $knownnetworks"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from station" \
    -xa "list $stations"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 2; and __fish_seen_subcommand_from wsc" \
    -xa "list $wsc"

# Third arguments
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 3; and __fish_seen_subcommand_from device" \
    -xa "set-property show"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 3; and __fish_seen_subcommand_from dpp" \
    -xa "start-configurator start-enrollee stop"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 3; and __fish_seen_subcommand_from known-networks" \
    -xa "forget set-property show"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 3; and __fish_seen_subcommand_from wsc" \
    -xa "cancel push-button start-pin start-user-pin"

# Fourth arguement
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 4; and __fish_seen_subcommand_from device set-property" \
    -xa  "Powered Mode"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 4; and __fish_seen_subcommand_from known-networks set-property" \
    -xa  "AutoConnect"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 4; and __fish_seen_subcommand_from station connect" \
    -xa  '(iwctl station wlan0 get-networks | tail -n -2 | awk \'{print $2}\')'
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 4; and __fish_seen_subcommand_from connect" \
    -xa  "$net"

# Fifth arguement
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 5; and __fish_seen_subcommand_from device set-property Mode" \
    -xa  "ad-hoc ap station"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 5; and __fish_seen_subcommand_from device set-property Powered" \
    -xa  "on off"
complete -c iwctl -n "test (__fish_number_of_cmd_args_wo_opts) = 5; and __fish_seen_subcommand_from known-networks set-property AutoConnect" \
    -xa  "yes no"
