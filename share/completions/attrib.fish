function __attrib_generate_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  switch $current_token
    case '+*'
      echo -e 'r\tSet the Read-only file attribute
a\tSet the Archive file attribute
s\tSet the System file attribute
h\tSet the Hidden file attribute
i\tSet the Not Content Indexed file attribute' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '-*'
      echo -e 'r\tUnset the Read-only file attribute
a\tUnset the Archive file attribute
s\tUnset the System file attribute
h\tUnset the Hidden file attribute
i\tUnset the Not Content Indexed file attribute' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '*'
      echo -e '/s\tApply changes recursively
/d\tApply changes only to directories
/l\tApply changes to symbolic links and not their targets
/?\tShow help'
  end
end

complete --command attrib --no-files --arguments '(__attrib_generate_args)'