complete -c rfkill -xa 'block unblock list event help' -n 'not __fish_seen_subcommand_from block unblock list event help'
complete -c rfkill -n '__fish_seen_subcommand_from block unblock list' -d 'device group' -xa "all wifi wlan bluetooth uwb ultrawideband wimax wwan gps fm (rfkill list | tr : \t)"
complete -c rfkill -l version -d 'Print version'

