set -l __fish_udevil_commands mount unmount umount clean monitor help info
complete -c udevil -f
complete -c udevil -n "not __fish_seen_subcommand_from $__fish_udevil_commands" -a "$__fish_udevil_commands"
complete -c udevil -n "not __fish_seen_subcommand_from $__fish_udevil_commands" -l verbose -l quiet
complete -c udevil -n "__fish_seen_subcommand_from mount" -a "(__fish_complete_blockdevice)" -F
complete -c udevil -n "__fish_seen_subcommand_from mount" -x -o t -a "(__fish_print_filesystems)"
complete -c udevil -n "__fish_seen_subcommand_from mount" -x -o o -a "(__fish_complete_mount_opts)"
complete -c udevil -n "__fish_seen_subcommand_from umount unmount" -a "(__fish_print_mounted)"
complete -c udevil -n "__fish_seen_subcommand_from umount unmount" -o l -o f
complete -c udevil -n "__fish_seen_subcommand_from info" -a "(__fish_complete_blockdevice)"
