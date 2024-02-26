complete -c iconv -s f -l from-code -d "Convert from specified encoding" -x -a "(__fish_print_encodings)"
complete -c iconv -s t -l to-code -d "Convert to specified encoding" -x -a "(__fish_print_encodings)"
complete -c iconv -l list -d "List known coded character sets"
complete -c iconv -s o -l output -d "Output file" -r
complete -c iconv -l verbose -d "Print progress information"
complete -c iconv -l help -d "Display version and exit"
complete -c iconv -l version -d "Display help and exit"
