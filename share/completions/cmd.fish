function __cmd_complete_args -d 'Function to generate args'
    set -l current_token (commandline -tc)

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
    end
end

complete -c cmd -f -a '(__cmd_complete_args)'

complete -c cmd -f -n 'not __fish_seen_argument -w c -w k' -a /c \
    -d 'Carry out the command specified by string and then stop'
complete -c cmd -f -n 'not __fish_seen_argument -w c -w k' -a /k \
    -d 'Carry out the command specified by string and continue'

complete -c cmd -f -a /s -d 'Modify the treatment of string after /c or /k'
complete -c cmd -f -a /q -d 'Turn the echo off'
complete -c cmd -f -a /d -d 'Disable execution of AutoRun commands'

complete -c cmd -f -n 'not __fish_seen_argument -w a -w u' -a /a \
    -d 'Format internal command output as ANSI'
complete -c cmd -f -n 'not __fish_seen_argument -w a -w u' -a /u \
    -d 'Format internal command output as Unicode'

complete -c cmd -f -a /t -d 'Set the background and foreground color'
complete -c cmd -f -a /e -d 'Manage command extensions'
complete -c cmd -f -a /f -d 'Manage file and directory name completion'
complete -c cmd -f -a /v -d 'Manage delayed environment variable expansion'
complete -c cmd -f -a '/?' -d 'Show help'
