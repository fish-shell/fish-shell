complete -c bunzip2 -x -a "(
	__fish_complete_suffix (commandline -ct) .bz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .bz2 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tbz 'Compressed archive'
	__fish_complete_suffix (commandline -ct) .tbz2 'Compressed archive'
)
"

complete -c bunzip2 -s c -l stdout --description "Decompress to stdout"
complete -c bunzip2 -s f -l force --description "Overwrite"
complete -c bunzip2 -s k -l keep --description "Do not overwrite"
complete -c bunzip2 -s s -l small --description "Reduce memory usage"
complete -c bunzip2 -s v -l verbose --description "Print compression ratios"
complete -c bunzip2 -s L -l license --description "Print license"
complete -c bunzip2 -s V -l version --description "Display version and exit"
