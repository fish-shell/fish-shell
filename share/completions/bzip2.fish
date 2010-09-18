complete -c bzip2 -s c -l stdout --description "Compress to stdout"
complete -c bzip2 -s d -l decompress -x -a "(
	__fish_complete_suffix .tbz
	__fish_complete_suffix .tbz2
)
"

complete -c bzip2 -s d -l decompress -x -a "(
	__fish_complete_suffix .bz
	__fish_complete_suffix .bz2
)
"

complete -c bzip2 -s z -l compress --description "Compress file"
complete -c bzip2 -s t -l test --description "Check integrity"
complete -c bzip2 -s f -l force --description "Overwrite"
complete -c bzip2 -s k -l keep --description "Do not overwrite"
complete -c bzip2 -s s -l small --description "Reduce memory usage"
complete -c bzip2 -s q -l quiet --description "Supress errors"
complete -c bzip2 -s v -l verbose --description "Print compression ratios"
complete -c bzip2 -s L -l license --description "Print license"
complete -c bzip2 -s V -l version --description "Display version and exit"
complete -c bzip2 -s 1 -l fast --description "Small block size"
complete -c bzip2 -s 9 -l best --description "Large block size"
