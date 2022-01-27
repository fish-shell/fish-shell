function __attributes_disk_complete_args
    if not __fish_seen_subcommand_from set clear
        echo -e 'set\tSet the specified attribute of the disk with focus
clear\tClear the specified attribute of the disk with focus'
        return
    end

    echo -e 'readonly\tSpecify that the disk is read-only
noerr\tWhen an error is encountered, DiskPart continues to process commands'
end

function __attributes_volume_complete_args
    if not __fish_seen_subcommand_from set clear
        echo -e 'set\tSet the specified attribute of the volume with focus
clear\tClear the specified attribute of the volume with focus'
        return
    end

    if not __fish_seen_subcommand_from readonly hidden nodefaultdriveletter shadowcopy
        echo -e 'readonly\tSpecify that the volume is read-only
hidden\tSpecify that the volume is hidden
nodefaultdriveletter\tSpecify that the volume does not receive a drive letter by default
shadowcopy\tSpecify that the volume is a shadow copy volume'
    end

    echo -e 'noerr\tWhen an error is encountered, DiskPart continues to process commands'
end

function __attributes_complete_args -d 'Function to generate args'
    if __fish_seen_subcommand_from disk
        __attributes_disk_complete_args
    else if __fish_seen_subcommand_from volume
        __attributes_volume_complete_args
    end
end

complete -c attributes -f -a '(__attributes_complete_args)'

complete -c attributes -f -n 'not __fish_seen_subcommand_from disk volume' -a disk \
    -d 'Display, set, or clear the attributes of a disk'
complete -c attributes -f -n 'not __fish_seen_subcommand_from dick volume' -a volume \
    -d 'Display, set, or clear the attributes of a volume'
