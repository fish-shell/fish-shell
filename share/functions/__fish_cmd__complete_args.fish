function __fish_cmd__complete_args -d 'Function to generate args'
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
