function __fish_fastboot_no_subcommand -d 'Test if fastboot has yet to be given the subcommand'
    for i in (commandline -opc)
        if contains -- $i flashall getvar oem flashing reboot update erase format devices flash get_staged help stage boot fetch
            return 1
        end
    end
    return 0
end

function __fish_fastboot_list_partition
    set -l partitions boot bootloader dtbo modem odm odm_dlkm oem product pvmfw radio recovery system vbmeta vendor vendor_dlkm cache userdata system_ext
    for i in $partitions
        echo $i
    end
end

function __fish_fastboot_list_partition_and_file
    for i in (commandline -opc)
        if contains -- (string replace -r '_[ab]' '' $i) (__fish_fastboot_list_partition)
            __fish_complete_path
        else
            __fish_fastboot_list_partition
        end
    end
end


complete -c fastboot -s h -l help -d 'Show this message'
complete -c fastboot -s v -l verbose -d 'Verbose output'
complete -c fastboot -l version -d 'Display version'

complete -n __fish_fastboot_no_subcommand -c fastboot -s w -d 'Wipe userdata'
complete -n __fish_fastboot_no_subcommand -c fastboot -s s -d 'Specify a device'
complete -n __fish_fastboot_no_subcommand -c fastboot -s S -d 'Break into sparse files no larger than SIZE'
complete -n __fish_fastboot_no_subcommand -c fastboot -l slot -d 'Use SLOT; \'all\' for both slots, \'other\' for non-current slot (default: current active slot)'
complete -n __fish_fastboot_no_subcommand -c fastboot -l set-active -d 'Sets the active slot before rebooting'

complete -f -n __fish_fastboot_no_subcommand -c fastboot -a flashall -d 'Flash all partitions from $ANDROID_PRODUCT_OUT'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a getvar -d 'Display given bootloader variable'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a oem -d 'Execute OEM-specific command'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a flashing -d 'Lock/unlock'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a reboot -d 'Reboot device'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a update -d 'Flash all partitions from an update.zip package'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a erase -d 'Erase a flash partition'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a format -d 'Format a flash partition'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a devices -d 'List devices in bootloader (-l: with device paths)'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a flash -d 'Flash given partition, using the image from $ANDROID_PRODUCT_OUT if no filename is given'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a help -d 'Show help message'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a stage -d 'Sends given file to stage for the next command'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a get_staged -d 'Writes data staged by the last command to a file'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a fetch -d 'Fetch a partition image from the device'
complete -f -n __fish_fastboot_no_subcommand -c fastboot -a boot -d 'Download and boot kernel from RAM'

# flash
complete -n '__fish_seen_subcommand_from flash' -c fastboot -f -a "(__fish_fastboot_list_partition_and_file)"
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l skip-secondary -d 'Don\'t flash secondary slots in flashall/update'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l skip-reboot -d 'Don\'t reboot device after flashing'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l disable-verity -d 'Sets disable-verity when flashing vbmeta'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l disable-verification -d 'Sets disable-verification when flashing vbmeta'
complete -n '__fish_seen_subcommand_from flash' -c fastboot -l force -d 'Force a flash operation that may be unsafe'

# boot
complete -n '__fish_seen_subcommand_from boot' -c fastboot

# fetch
complete -n '__fish_seen_subcommand_from fetch' -c fastboot -f -a "(__fish_fastboot_fetch)"

# devices
complete -n '__fish_seen_subcommand_from devices' -c fastboot -s l -d 'device paths'

# format
complete -n '__fish_seen_subcommand_from format' -c fastboot -f -a "(__fish_fastboot_list_partition_and_file)"

# erase
complete -n '__fish_seen_subcommand_from erase' -c fastboot -f -a "(__fish_fastboot_list_partition)"

# flashing
complete -n '__fish_seen_subcommand_from flashing' -c fastboot -f -a "lock unlock lock_critical unlock_critical get_unlock_ability"
