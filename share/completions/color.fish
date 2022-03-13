function __color_generate_args --description 'Function to generate args'
    set --local current_token (commandline --current-token --cut-at-cursor)
    switch $current_token
        case '/*'
            echo -e '/?\tShow help'
        case '*'
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
    end
end

complete --command color --no-files --arguments '(__color_generate_args)'
