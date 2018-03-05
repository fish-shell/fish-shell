
# magic completion safety check (do not remove this comment)
if not type -q cut
    exit
end
complete -c cut -x
complete -c cut -s b -l bytes -x -d "Output byte range"
complete -c cut -s c -l characters -x -d "Output character range"
complete -c cut -s d -l delimiter -x -d "Select field delimiter"
complete -c cut -s d -l fields -x -d "Select fields"
complete -c cut -s n -d "Don't split multi byte characters"
complete -c cut -s s -l only-delimited -d "Do not print lines without delimiter"
complete -c cut -l output-delimiter -d "Select output delimiter"
complete -c cut -l help -d "Display help and exit"
complete -c cut -l version -d "Display version and exit"

