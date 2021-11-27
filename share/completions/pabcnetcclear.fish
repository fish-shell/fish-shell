function __pabcnetcclear_generate_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  switch $current_token
    case '/Debug:*'
      echo -e $current_token"0\tEnable generation
$current_token"1"\tDisable generation"
    case '*'
      echo -e "/Help\tShow help
/H\tShow help
/?\tShow help
/Debug\tGenerate a code debug info
/output\tSpecify an executable name
/SearchDir\tSpecify a path to search units"
  end
end

complete --command pabcnetcclear --no-files --arguments '(__pabcnetcclear_generate_args)'
