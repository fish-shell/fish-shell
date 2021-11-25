function __cmd_generate_args --description 'Function to generate args'
  set --local current_token (commandline --current-token --cut-at-cursor)
  switch $current_token
    case '/t:*'
      echo -e '0\tBlack
1\tBlue
2\tGreen
3\tAqua
4\tRed
5\tPurple
6\tYellow
7\tWhite
8\tGray
9\tLight blue
A\tLight green
B\tLight aqua
C\tLight red
D\tLight purple
E\tLight yellow
F\tBright white' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '/e:*'
      echo -e 'on\tEnable command extensions
off\tDisable command extensions' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '/f:*'
      echo -e 'on\tEnable file and directory name completion
off\tDisable file and directory name completion' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '/v:*'
      echo -e 'on\tEnable delayed environment variable expansion
off\tDisable delayed environment variable expansion' | awk -F '\t' "{ printf \"$current_token%s\t%s\n\", \$1, \$2 }"
    case '*'
      echo -e '/c\tCarry out the command specified by string and then stop
/k\tCarry out the command specified by string and continue
/s\tModify the treatment of string after /c or /k
/q\tDisable echo
/d\tDisables execution of AutoRun commands
/a\tFormat internal command output to a pipe or a file as ANSI
/u\tFormat internal command output to a pipe or a file as Unicode
/t\tSet the background and foreground color
/e\tManage command extensions
/f\tManage file and directory name completion
/v\tManage delayed environment variable expansion
/?\tShow help'
  end
end

complete --command cmd --no-files --arguments '(__cmd_generate_args)'
