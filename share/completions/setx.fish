function __setx_complete_args --description 'Function to generate args'
  set --local previous_token (commandline --tokenize --cut-at-cursor)[-1]

  if test "$previous_token" = '/u'
    __fish_list_windows_users
    return
  end

  if __fish_seen_argument --windows 's'
    echo -e '/u\tRun the script with the credentials of the specified user account'
  end

  if __fish_seen_argument --windows 'u'
    echo -e '/p\tSpecify the password of the user account that is specified in the /u parameter'
  end

  if not __fish_seen_argument --windows 'a' --windows 'r' --windows 'x'
    echo -e '/a\tSpecify absolute coordinates and offset as search parameters
/r\tSpecify relative coordinates and offset
/x\tDisplay file coordinates, ignoring the /a, /r, and /d command-line options'
  end

  if __fish_seen_argument --windows 'a' --windows 'r'
    echo -e '/m\tSpecify to set the variable in the system environment'
  end

  echo -e '/s\tSpecify the name or IP address of a remote computer
/k\tSpecify that the variable is set based on information from a registry key
/f\tSpecify the file that you want to use
/d\tSpecify delimiters to be used
/?\tShow help'
end

complete --command setx --no-files --arguments '(__setx_complete_args)'
