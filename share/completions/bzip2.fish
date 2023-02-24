complete -c bzip2 -s c -l stdout -d "Compress to stdout"
complete -c bzip2 -s d -l decompress -k -x -a "(__fish_complete_suffix .tbz .tbz2 .bz .bz2)"

complete -c bzip2 -s z -l compress -d "Compress file"
complete -c bzip2 -s t -l test -d "Check integrity"
complete -c bzip2 -s f -l force -d Overwrite
complete -c bzip2 -s k -l keep -d "Do not overwrite"
complete -c bzip2 -s s -l small -d "Reduce memory usage"
complete -c bzip2 -s q -l quiet -d "Suppress errors"
complete -c bzip2 -s v -l verbose -d "Print compression ratios"
complete -c bzip2 -s L -l license -d "Print license"
complete -c bzip2 -s V -l version -d "Display version and exit"
complete -c bzip2 -s 1 -l fast -d "Small block size"
complete -c bzip2 -s 9 -l best -d "Large block size"
