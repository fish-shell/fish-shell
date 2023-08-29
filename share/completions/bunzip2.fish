complete -c bunzip2 -k -x -a "(__fish_complete_suffix .tbz .tbz2 .bz2 .bz)"

complete -c bunzip2 -s c -l stdout -d "Decompress to stdout"
complete -c bunzip2 -s f -l force -d Overwrite
complete -c bunzip2 -s k -l keep -d "Do not overwrite"
complete -c bunzip2 -s s -l small -d "Reduce memory usage"
complete -c bunzip2 -s v -l verbose -d "Print compression ratios"
complete -c bunzip2 -s L -l license -d "Print license"
complete -c bunzip2 -s V -l version -d "Display version and exit"
