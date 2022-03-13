function __attrib_complete_args -d 'Function to generate args'
    set -l current_token (commandline -tc)

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
    end
end

complete -c attrib -f -a '(__attrib_complete_args)'

complete -c attrib -f -n '__fish_seen_argument -w s' -a d \
    -d 'Apply attrib and any command-line options to directories'
complete -c attrib -f -n '__fish_seen_argument -w s' -a l \
    -d 'Apply attrib and any command-line options to the Symbolic Link'

complete -c attrib -f -a + -d 'Set the file attribute'
complete -c attrib -f -a - -d 'Clear the file attribute'
complete -c attrib -f -a /s \
    -d 'Apply to matching files in the current directory and all of its subdirectories'
complete -c attrib -f -a '/?' -d 'Show help'
