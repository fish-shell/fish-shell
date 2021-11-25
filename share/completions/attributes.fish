function __attributes_generate_args --description 'Function to generate args'
  if ! __fish_seen_subcommand_from disk volume
    echo -e 'disk\tManipulate disk attributes
volume\tManipulate volume attributes'
    return
  end

  if __fish_seen_subcommand_from disk
    if ! __fish_seen_subcommand_from set clear
      echo -e 'set\tSet an attribute of the disk with focus
clear\tUnset an attribute of the disk with focus'
      return
    end

    echo -e 'readonly\tUse readonly attribute
noerr\tSuppress errors for DiskPart'
    return
  end

  if __fish_seen_subcommand_from volume
    if ! __fish_seen_subcommand_from set clear
      echo -e 'set\tSet an attribute of the volume with focus
clear\tUnset an attribute of the disk volume focus'
      return
    end

    echo -e 'readonly\tUse readonly attribute
readonly\tUse hidden attribute
nodefaultdriveletter\tDon\'t use a drive letter by default
shadowcopy\tSet volume type to shadow copy volume
noerr\tSuppress errors for DiskPart'
    return
  end
end

complete --command attributes --no-files --arguments '(__attributes_generate_args)'