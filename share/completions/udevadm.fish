set -l commands info trigger settle control monitor test test-builtin
complete -c udevadm -n "__fish_is_nth_token 1" -xa "$commands"

complete -c udevadm -s h -l help -d "Show help"
complete -c udevadm -s V -l version -d "Show version info"

# udevadm info
set -l query_type "name\t'Name of device node'" "symlink\t'Pointing to node'" "path\t'sysfs device path'" "property\t'Device properties'" all
complete -c udevadm -n '__fish_seen_subcommand_from info' -s q -l query -d "Query device information" -xa "$query_type"
complete -c udevadm -n '__fish_seen_subcommand_from info' -s p -l path -d "sysfs device path" -f
complete -c udevadm -n '__fish_seen_subcommand_from info' -s n -l name -d "Node or symlink name"
complete -c udevadm -n '__fish_seen_subcommand_from info' -s r -l root -d "Prepend dev directory"
complete -c udevadm -n '__fish_seen_subcommand_from info' -s a -l attribute-walk -d "Print all key matches"
complete -c udevadm -n '__fish_seen_subcommand_from info' -s d -l device-id-of-file -d "Print major:minor of device containing FILE" -f
complete -c udevadm -n '__fish_seen_subcommand_from info' -s x -l export -d "Export key/value pairs"
complete -c udevadm -n '__fish_seen_subcommand_from info' -s P -l export-prefix -d "Export key name with prefix"
complete -c udevadm -n '__fish_seen_subcommand_from info' -s e -l export-db -d "Export content of udev database"
complete -c udevadm -n '__fish_seen_subcommand_from info' -s c -l cleanup-db -d "Clean up the udev database"

# udevadm trigger
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s v -l verbose -d "Print device paths while running"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s n -l dry-run -d "Do not actually trigger"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s t -l type -d "Type of events to trigger" -xa "devices\t'sysfs devices' subsystems\t'sysfs subsystems and drivers'"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s c -l action -d "Event action value [change]"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s s -l subsystem-match -d "Trigger devices matching subsystem"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s S -l subsystem-nomatch -d "Exclude devices matching subsystem"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s a -l attr-match -d "Trigger devices matching attribute"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s A -l attr-nomatch -d "Exclude devices matching attribute"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s p -l property-match -d "Trigger devices matching property"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s g -l tag-match -d "Trigger devices matching property"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s y -l sysname-match -d "Trigger devices matching /sys path"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -l name-match -d "Trigger devices matching /dev path"
complete -c udevadm -n '__fish_seen_subcommand_from trigger' -s b -l parent-match -d "Trigger devices with parent device"

# udevadm settle
complete -c udevadm -n '__fish_seen_subcommand_from settle' -s t -l timeout -d "Maximum time in seconds to wait"
complete -c udevadm -n '__fish_seen_subcommand_from settle' -s E -l exit-if-exists -d "Stop waiting if FILE exists"

# udevadm control
complete -c udevadm -n '__fish_seen_subcommand_from control' -s e -l exit -d "Exit the daemon"
# todo: enumerate log levels
complete -c udevadm -n '__fish_seen_subcommand_from control' -s l -l log-priority -d "Set udev daemon log level"
complete -c udevadm -n '__fish_seen_subcommand_from control' -s s -l stop-exec-queue -d "Queue but do not execute events"
complete -c udevadm -n '__fish_seen_subcommand_from control' -s S -l start-exec-queue -d "Execute events, flush queue"
complete -c udevadm -n '__fish_seen_subcommand_from control' -s R -l reload -d "Reload rules and database"
complete -c udevadm -n '__fish_seen_subcommand_from control' -s p -l property -d "Set property globally"
complete -c udevadm -n '__fish_seen_subcommand_from control' -s m -l children-max -d "Max num of children"
complete -c udevadm -n '__fish_seen_subcommand_from control' -s t -l timeout -d "Max seconds to block for reply"

# udevadm monitor
complete -c udevadm -n '__fish_seen_subcommand_from monitor' -s p -l property -d "Print event properties"
complete -c udevadm -n '__fish_seen_subcommand_from monitor' -s k -l kernel -d "Print kernel uevents"
complete -c udevadm -n '__fish_seen_subcommand_from monitor' -s u -l udev -d "Print udev events"
complete -c udevadm -n '__fish_seen_subcommand_from monitor' -s s -l subsystem-match -d "Filter events by subsystem"
complete -c udevadm -n '__fish_seen_subcommand_from monitor' -s t -l tag-match -d "Filter events by tag"

# udevadm test
complete -c udevadm -n '__fish_seen_subcommand_from test' -s a -l action -d "Set action string"
complete -c udevadm -n '__fish_seen_subcommand_from test' -s N -l resolve-names -d "When to resolve names" -xa "early late never"

# udevadm test-builtin
complete -c udevadm -n '__fish_seen_subcommand_from test-builtin; and __fish_is_nth_token 2' -xa "blkid\t'Filesystem and partition probing' btrfs\t'btrfs volume management' hwdb\t'Hardware database' input_id\t'Input device properties' keyboard\t'Keybord scan code to key mapping' kmod\t'Kernel module loader' net_id\t'Network device properties' net_setup_link\t'Configure network link' path_id\t'Compose persistent device path' usb_id\t'USB device properties' uaccess\t'Manage device node user ACL'"
