complete -c rfkill -xa 'block unblock list event help' -n 'not __fish_seen_subcommand_from block unblock list event help'
complete -c rfkill -n '__fish_seen_subcommand_from block unblock list' -d 'device group' -xa "all wifi wlan bluetooth uwb ultrawideband wimax wwan gps fm (rfkill list | string replace : \t)"
complete -c rfkill -s V -l version -d 'Print version'
complete -c rfkill -s h -l help -d 'Print help'
complete -c rfkill -s J -l json -d 'JSON output'
complete -c rfkill -s r -l raw -d 'Raw output'
complete -c rfkill -s n -l noheadings -d "Don't print headings"
complete -c rfkill -s o -l output -d "Columns to output" -xa "DEVICE ID TYPE TYPE-DESC SOFT HARD"
complete -c rfkill -l output-all -d "Output all columns"
