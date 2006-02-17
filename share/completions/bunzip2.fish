complete -c bunzip2 -x -a "(
	__fish_complete_suffix (commandline -ct) .bz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .bz2 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tbz 'Compressed archive'
	__fish_complete_suffix (commandline -ct) .tbz2 'Compressed archive'
)
"

complete -c bunzip2 -s c -l stdout -d (_ "Decompress to stdout")
complete -c bunzip2 -s f -l force -d (_ "Overwrite")
complete -c bunzip2 -s k -l keep -d (_ "Do not overwrite")
complete -c bunzip2 -s s -l small -d (_ "Reduce memory usage")
complete -c bunzip2 -s v -l verbose -d (_ "Print compression ratios")
complete -c bunzip2 -s L -l license -d (_ "Print license")
complete -c bunzip2 -s V -l version -d (_ "Display version and exit")
