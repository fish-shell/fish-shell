

complete -c iconv -s f -l from-code --description "Convert from specified encoding" -x -a "(__fish_print_encodings)"
complete -c iconv -s t -l to-code --description "Convert to specified encoding" -x -a "(__fish_print_encodings)"
complete -c iconv -l list --description "List known coded character sets"
complete -c iconv -s o -l output --description "Output file" -r
complete -c iconv -l verbose --description "Print progress information"
complete -c iconv -l help --description "Display version and exit"
complete -c iconv -l version --description "Display help and exit"

