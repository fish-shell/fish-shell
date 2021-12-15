function __cleanmgr_generate_args --description 'Function to generate args'
  set --local previous_token (commandline --tokenize --cut-at-cursor)[-1]

  if test "$previous_token" = '/d'
    __fish_list_windows_drives
    return
  end

  echo -e '/d\tSpecify the drive
/sageset\tDisplay the Disk Cleanup Settings dialog box and also create a registry keyt
/sagerun\tRun the specified tasks that are assigned to the n value
/tuneup\tRun /sageset and /sagerun for the same n
/lowdisk\tRun with the default settings
/verylowdisk\tRun with the default settings, no user prompts
/?\tShow help'
end

complete --command cleanmgr --no-files --arguments '(__cleanmgr_generate_args)'
