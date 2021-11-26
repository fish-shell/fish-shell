function __rd_generate_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  switch $current_token
    case '/*'
      if __fish_seen_argument --windows 's'
        echo -e '/q\tSpecify quiet mode'
      end

      echo -e '/s\tDelete a directory tree
/?\tShow help'
    case '*'
      __fish_list_windows_drives
  end
end

complete --command rd --no-files --arguments '(__rd_generate_args)'
