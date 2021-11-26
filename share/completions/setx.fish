function __setx_generate_args --description 'Function to generate args'
  set --local previous_token (commandline --tokenize --cut-at-cursor)[-1]

  if test "$previous_token" = '/u'
    __fish_list_windows_users
    return
  end

  if not __fish_seen_argument --windows 'a' --windows 'r' --windows 'x'
    echo -e '/a\tSpecify absolute coordinates and offset as search parameters
/r\tSpecify relative coordinates and offset from String as search parameters
/x\tDisplay file coordinates, ignoring the /a, /r, and /d command-line options'
  end 

  if __fish_seen_argument --windows 'a' --windows 'r'
    echo -e '/m\tSpecify to set the variable in the system environment'
  end

  echo -e '/s\tSpecify the name or IP address of a remote computer
/u\tRun the script with the credentials of the specified user account
/p\tSpecify the password of the user account that is specified in the /u parameter
/k\tSpecify that the variable is set based on information from a registry key
/f\tSpecify the file that you want to use
/d\tSpecify delimiters to be used in addition to the four built-in delimiters
/?\tShow help'
end

complete --command setx --no-files --arguments '(__setx_generate_args)'