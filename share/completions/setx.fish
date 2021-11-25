function __setx_generate_args --description 'Function to generate args'
  echo -e '/u\tSpecify the name or IP address of a remote computer
/p\tSpecify the password of the user account that is specified in the /u parameter
/f\tSpecify the file that you want to use
/d\tSpecify delimiters to be used in addition to the four built-in delimiters
/?\tShow help'

  if not __fish_seen_argument --windows 'a' --windows 'm' --windows 'x'
    echo -e '/a\tSpecify absolute coordinates and offset as search parameters
/m\tSpecify to set the variable in the system environment
/x\tDisplay file coordinates, ignoring the /a, /r, and /d command-line options'
  end
end

complete --command setx --no-files --arguments '(__setx_generate_args)'