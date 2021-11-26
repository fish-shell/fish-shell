function __md_generate_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  switch $current_token
    case '/*'
      echo -e '/?\tShow help'
    case '*'
      __fish_list_windows_drives
  end
end

complete --command md --no-files --arguments '(__md_generate_args)'
