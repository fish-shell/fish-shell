function __attributes_disk_generate_args
  if not __fish_seen_subcommand_from set clear
    echo -e 'set\tSet the specified attribute of the disk with focus
clear\tClear the specified attribute of the disk with focus'
    return
  end

  echo -e 'readonly\tSpecify that the disk is read-only
noerr\tWhen an error is encountered, DiskPart continues to process commands'
end

function __attributes_volume_generate_args
  if not __fish_seen_subcommand_from set clear
    echo -e 'set\tSet the specified attribute of the volume with focus
clear\tClear the specified attribute of the volume with focus'
    return
  end

  echo -e 'readonly\tSpecify that the volume is read-only
readonly\tSpecify that the volume is hidden
nodefaultdriveletter\tSpecify that the volume does not receive a drive letter by default
shadowcopy\tSpecify that the volume is a shadow copy volume
noerr\tWhen an error is encountered, DiskPart continues to process commands'
end

function __attributes_generate_args --description 'Function to generate args'
  if not __fish_seen_subcommand_from disk volume
    echo -e 'disk\tDisplay, set, or clear the attributes of a disk
volume\tDisplay, set, or clear the attributes of a volume'
    return
  end

  if __fish_seen_subcommand_from disk
    __attributes_disk_generate_args
  else if __fish_seen_subcommand_from volume
    __attributes_volume_generate_args
  end
end

complete --command attributes --no-files --arguments '(__attributes_generate_args)'