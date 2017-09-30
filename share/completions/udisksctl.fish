set -l cmds help info dump status monitor mount unmount unlock lock loop-setup loop-delete power-off smart-simulate

function __fish_print_mounted_blockdevice
    if test -r /proc/mounts
        string match -r "^/[^ ]*" </proc/mounts
    end
end

complete -f -c udisksctl -n "__fish_seen_subcommand_from $cmds" -l help -d "Shows help"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a help -d "Shows help"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a info -d "Shows information about an object"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a dump -d "Shows information about all objects"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a status -d "Shows high-level status"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a monitor -d "Monitor changes to objects"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a mount -d "Mount a filesystem"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a unmount -d "Unmount a filesystem"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a unlock -d "Unlock an encrypted device"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a lock -d "Lock an encrypted device"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a loop-setup -d "Set-up a loop device"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a loop-delete -d "Delete a loop device"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a power-off -d "Safely power off a drive"
complete -f -c udisksctl -n "not __fish_seen_subcommand_from $cmds" -a smart-simulate -d "Set SMART data for a drive"

complete -f -c udisksctl -n "__fish_seen_subcommand_from info" -s p -l object-path -d "Object to get information about"
complete -c udisksctl -n "__fish_seen_subcommand_from info" -s b -l block-device -d "Block device to get information about" -a "(__fish_complete_blockdevice)"
complete -f -c udisksctl -n "__fish_seen_subcommand_from info" -s d -l drive -d "Drive to get information about"

complete -x -c udisksctl -n "__fish_seen_subcommand_from mount" -s t -l filesystem-type -d "Filesystem type to use" -a "(__fish_print_filesystems)"
complete -x -c udisksctl -n "__fish_seen_subcommand_from mount" -s o -l options -d "Mount options" -a '(__fish_complete_mount_opts)'

complete -c udisksctl -n "__fish_seen_subcommand_from unmount" -s f -l force -d "Force/layzy unmount"
complete -c udisksctl -n "__fish_seen_subcommand_from unmount" -a "(__fish_print_mounted)" -f -d "Mount point"
complete -c udisksctl -n "__fish_seen_subcommand_from unmount" -s b -l block-device -a "(__fish_print_mounted_blockdevice)" -x -d "Mounted block device"

complete -c udisksctl -n "__fish_seen_subcommand_from loop-setup" -s f -l file -d "File to set-up a loop device for"
complete -c udisksctl -n "__fish_seen_subcommand_from loop-setup" -s r -l read-only -d "Setup read-only device"
complete -c udisksctl -n "__fish_seen_subcommand_from loop-setup" -s o -l offset -x -d "Start at <num> bytes into file"
complete -c udisksctl -n "__fish_seen_subcommand_from loop-setup" -s s -l size -x -d "Limit size to <num> bytes"

complete -c udisksctl -n "__fish_seen_subcommand_from loop-delete" -s p -l object-path -d "Object for loop device to delete"
complete -c udisksctl -n "__fish_seen_subcommand_from loop-delete" -s b -l block-device -d "Loop device to delete"

complete -c udisksctl -n "__fish_seen_subcommand_from smart-simulate" -s f -l file -d "File with libatasmart blob"

complete -c udisksctl -n "__fish_seen_subcommand_from mount unmount lock unlock loop-setup loop-delete power-off smart-simulate" -l no-user-interaction -d "Do not authenticate the user if needed"

for cmd in mount lock unlock
    complete -r -c udisksctl -n "__fish_seen_subcommand_from $cmd" -s p -l object-path -d "Object to $cmd"
    complete -r -c udisksctl -n "__fish_seen_subcommand_from $cmd" -s b -l block-device -d "Block device to $cmd" -a "(__fish_complete_blockdevice)"
end

for cmd in power-off smart-simulate
    complete -r -c udisksctl -n "__fish_seen_subcommand_from $cmd" -s p -l object-path -d "Object path for ATA device"
    complete -r -c udisksctl -n "__fish_seen_subcommand_from $cmd" -s b -l block-device -d "Block device for ATA device" -a "(__fish_complete_blockdevice)"
end
