function __findstr_generate_args --description 'Function to generate args'
    set --local current_token (commandline --current-token --cut-at-cursor)
    switch $current_token
        case '/a:*'
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
        case '*'
            echo -e '/b\tMatch at the beginning of the line
/e\tMatch at the end of the line
/l\tUse literal strings
/r\tUse regular expressions
/s\tSearch a current directory and all subdirectories
/i\tIgnore case
/x\tMatch exactly
/v\tInvert match
/n\tPrint line numbers
/m\tPrint only the file name with matching lines
/o\tPrint character offset
/p\tSkip files with non-printable characters
/off\tDon\'t skip files that have the offline attribute set
/offline\tDon\'t skip files that have the offline attribute set
/f:\tRead a file list from file
/c:\tSpecify a literal search string
/g:\tRead literal search strings from file
/d:\tSpecify the search list of directories
/a:\tUse color attributes with two hexadecimal digits
/?\tShow help'
    end
end

complete --command findstr --no-files --arguments '(__findstr_generate_args)'
