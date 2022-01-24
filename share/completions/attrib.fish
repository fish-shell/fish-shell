function __attrib_complete_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)

  switch $current_token
    case '+*'
      echo -e 'r\tSet the Read-only file attribute
a\tSet the Archive file attribute
s\tSet the System file attribute
h\tSet the Hidden file attribute
i\tSet the Not Content Indexed file attribute' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '-*'
      echo -e 'r\tClear the Read-only file attribute
a\tClear the Archive file attribute
s\tClear the System file attribute
h\tClear the Hidden file attribute
i\tClear the Not Content Indexed file attribute' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '*'
      if __fish_seen_argument --windows 's'
        echo -e '/d\tApply attrib and any command-line options to directories
/l\tApply attrib and any command-line options to the Symbolic Link'
      end

      echo -e '+\tSet the file attribute
-\tClear the file attribute
/s\tApply to matching files in the current directory and all of its subdirectories
/?\tShow help'
  end
end

complete --command attrib --no-files --arguments '(__attrib_complete_args)'
