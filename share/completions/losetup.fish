# losetup - Set up and control loop devices.
#
# This is part of the util-linux package.
# https://www.kernel.org/pub/linux/utils/util-linux

function __fish_print_losetup_list_output
    printf "%s\t%s\n" \
        NAME "Loop device name" \
        AUTOCLEAR "Autoclear flag set" \
        BACK-FILE "Device backing file" \
        BACK-INO "Backing file inode number" \
        BACK-MAJ:MIN "Backing file major:minor device number" \
        MAJ:MIN "Loop device major:minor number" \
        OFFSET "Offset from the beginning" \
        PARTSCAN "Partscan flag set" \
        RO "Read-only device" \
        SIZELIMIT "Size limit of the file in bytes" \
        DIO "Access backing file with direct-io" \
        LOG-SEC "Logical sector size in bytes"
end

function __fish_print_losetup_attached
    losetup --list --raw --noheadings --output NAME,BACK-FILE | string replace ' ' \t
end

complete -c losetup -s a -l all -d "List all used devices"
complete -c losetup -s d -l detach -x -a "(__fish_print_losetup_attached)" -d "Detach one or more devices"
complete -c losetup -s D -l detach-all -d "Detach all used devices"
complete -c losetup -s f -l find -d "Find first unused device"
complete -c losetup -s c -l set-capacity -x -a "(__fish_print_losetup_attached)" -d "Resize the device"
complete -c losetup -s j -l associated -r -d "List all devices associated with given file"
complete -c losetup -s L -l nooverlap -d "Avoid possible conflict between devices"
complete -c losetup -s o -l offset -x -d "Start at given offset into file"
complete -c losetup -l sizelimit -x -d "Device is limited to give bytes of the file"
complete -c losetup -s b -l sector-size -x -d "Set the logical sector size"
complete -c losetup -s P -l partscan -d "Create a partitioned loop device"
complete -c losetup -s r -l read-only -d "Set up a read-only loop device"
complete -c losetup -l direct-io -x -a "on off" -d "open backing file with O_DIRECT"
complete -c losetup -l show -d "Print device name after setup"
complete -c losetup -s v -l verbose -d "Verbose mode"
complete -c losetup -s J -l json -d "Use JSON --list output format"
complete -c losetup -s l -l list -d "List info about all or specified"
complete -c losetup -s n -l noheadings -d "Don't print headings for --list output"
complete -c losetup -s O -l output -x -a "(__fish_complete_list , __fish_print_losetup_list_output)" -d "Specify columns to output for --list"
complete -c losetup -l output-all -d "Output all columns"
complete -c losetup -l raw -d "Use raw --list output format"
complete -c losetup -s h -l help -d "Display help"
complete -c losetup -s V -l version -d "Display version"
