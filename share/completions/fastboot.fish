set -l commands flashall getvar oem flashing reboot update erase format devices flash get_staged help stage boot fetch

function __fish_fastboot_list_partition_or_file
    set -l tokens (commandline -xpc)
    # if last 2 token is flash, then list file
    if test (count $tokens) -gt 2
        if test $tokens[-2] = flash
            # complete files
            __fish_complete_suffix .img
            __fish_complete_suffix $tokens[-1]
            return
        end
    end
    __fish_fastboot_list_partition
end

function __fish_fastboot_list_partition
    set -l partitions boot bootloader cache cust dtbo metadata misc modem odm odm_dlkm oem product pvmfw radio recovery system system_ext userdata vbmeta vendor vendor_dlkm vmbeta_system
    for i in $partitions
        echo $i
    end
end

complete -c fastboot -s h -l help -d 'Show this message'
complete -c fastboot -s v -l verbose -d 'Verbose output'
complete -c fastboot -l version -d 'Display version'

complete -n "not __fish_seen_subcommand_from $commands" -c fastboot -s w -d 'Wipe userdata'
complete -n "not __fish_seen_subcommand_from $commands" -c fastboot -s s -x -a "(fastboot devices)" -d 'Specify a device'
complete -n "not __fish_seen_subcommand_from $commands" -c fastboot -s S -d 'Break into sparse files no larger than SIZE'
complete -n "not __fish_seen_subcommand_from $commands" -c fastboot -l slot -d 'Use SLOT; \'all\' for both slots, \'other\' for non-current slot (default: current active slot)' -xa "all other a b"
complete -n "not __fish_seen_subcommand_from $commands" -c fastboot -l set-active -d 'Sets the active slot before rebooting' -xa "a b"

complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a flashall -d 'Flash all partitions from $ANDROID_PRODUCT_OUT'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a getvar -d 'Display given bootloader variable'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a oem -d 'Execute OEM-specific command'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a flashing -d Lock/unlock
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a reboot -d 'Reboot device'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a update -d 'Flash all partitions from an update.zip package'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a erase -d 'Erase a flash partition'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a format -d 'Format a flash partition'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a devices -d 'List devices in bootloader'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a flash -d 'Flash given partition'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a help -d 'Show help message'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a stage -d 'Sends given file to stage for the next command'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a get_staged -d 'Writes data staged by the last command to a file'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a fetch -d 'Fetch a partition image from the device'
complete -f -n "not __fish_seen_subcommand_from $commands" -c fastboot -a boot -d 'Download and boot kernel from RAM'

# flash
complete -n '__fish_seen_subcommand_from flash' -c fastboot -f -k -a "(__fish_fastboot_list_partition_or_file)"
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l skip-secondary -d 'Don\'t flash secondary slots in flashall/update'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l skip-reboot -d 'Don\'t reboot device after flashing'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l disable-verity -d 'Sets disable-verity when flashing vbmeta'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l disable-verification -d 'Sets disable-verification when flashing vbmeta'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l force -d 'Force a flash operation that may be unsafe'

# boot
complete -n '__fish_seen_subcommand_from boot' -c fastboot

# fetch
complete -n '__fish_seen_subcommand_from fetch' -c fastboot -f -a "(__fish_fastboot_list_partition)"

# devices
complete -n '__fish_seen_subcommand_from devices' -c fastboot -f
complete -n '__fish_seen_subcommand_from devices' -c fastboot -s l -d 'device paths'

# format
complete -n '__fish_seen_subcommand_from format' -c fastboot -f -a "(__fish_fastboot_list_partition)"

# erase
complete -n '__fish_seen_subcommand_from erase' -c fastboot -f -a "(__fish_fastboot_list_partition)"

# flashing
complete -n '__fish_seen_subcommand_from flashing' -c fastboot -f -a "lock unlock lock_critical unlock_critical get_unlock_ability"

# reboot
complete -n '__fish_seen_subcommand_from reboot' -c fastboot -xa 'bootloader fastboot'

# oem
complete -n '__fish_seen_subcommand_from oem' -c fastboot -xa 'device-info lock unlock edl'
