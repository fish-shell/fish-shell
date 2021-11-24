function __pabcnetcclear_generate_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  switch $current_token
    case '/Debug:*'
      echo -e $current_token"0\tEnable generation
$current_token"1"\tDisable generation"
    case '*'
      echo -e "/Help\tDisplay help
/H\tDisplay help
/?\tDisplay help
/Debug\tGenerate code with optimizations
/output\tExecutable name
/SearchDir\tAdd path to unit search directories"
  end
end

complete --command pabcnetcclear --no-files --arguments "(__pabcnetcclear_generate_args)"
