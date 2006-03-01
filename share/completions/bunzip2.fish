complete -c bunzip2 -x -a "(
	__fish_complete_suffix (commandline -ct) .bz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .bz2 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tbz 'Compressed archive'
	__fish_complete_suffix (commandline -ct) .tbz2 'Compressed archive'
)
"

complete -c bunzip2 -s c -l stdout -d (N_ "Decompress to stdout")
complete -c bunzip2 -s f -l force -d (N_ "Overwrite")
complete -c bunzip2 -s k -l keep -d (N_ "Do not overwrite")
complete -c bunzip2 -s s -l small -d (N_ "Reduce memory usage")
complete -c bunzip2 -s v -l verbose -d (N_ "Print compression ratios")
complete -c bunzip2 -s L -l license -d (N_ "Print license")
complete -c bunzip2 -s V -l version -d (N_ "Display version and exit")
