complete -c bunzip2 -x -a "(
	__fish_complete_suffix .tbz
	__fish_complete_suffix .tbz2
)
"

complete -c bunzip2 -x -a "(
	__fish_complete_suffix .bz
	__fish_complete_suffix .bz2
)
"

complete -c bunzip2 -s c -l stdout --description "Decompress to stdout"
complete -c bunzip2 -s f -l force --description "Overwrite"
complete -c bunzip2 -s k -l keep --description "Do not overwrite"
complete -c bunzip2 -s s -l small --description "Reduce memory usage"
complete -c bunzip2 -s v -l verbose --description "Print compression ratios"
complete -c bunzip2 -s L -l license --description "Print license"
complete -c bunzip2 -s V -l version --description "Display version and exit"
