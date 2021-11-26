function __rd_generate_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  switch $current_token
    case '/*'
      echo -e '/s\tDelete a directory tree
/q\tSpecify quiet mode
/?\tShow help'
    case '*'
      wmic logicaldisk get name | tail --lines +2
  end
end

complete --command rd --no-files --arguments '(__rd_generate_args)'
