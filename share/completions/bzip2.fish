complete -c bzip2 -s c -l stdout -d (N_ "Compress to stdout")
complete -c bzip2 -s d -l decompress -x -a "(
	__fish_complete_suffix (commandline -ct) .bz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .bz2 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tbz 'Compressed archive'
	__fish_complete_suffix (commandline -ct) .tbz2 'Compressed archive'
)
"
complete -c bzip2 -s z -l compress -d (N_ "Compress file")
complete -c bzip2 -s t -l test -d (N_ "Check integrity")
complete -c bzip2 -s f -l force -d (N_ "Overwrite")
complete -c bzip2 -s k -l keep -d (N_ "Do not overwrite")
complete -c bzip2 -s s -l small -d (N_ "Reduce memory usage")
complete -c bzip2 -s q -l quiet -d (N_ "Supress errors")
complete -c bzip2 -s v -l verbose -d (N_ "Print compression ratios")
complete -c bzip2 -s L -l license -d (N_ "Print license")
complete -c bzip2 -s V -l version -d (N_ "Display version and exit")
complete -c bzip2 -s 1 -l fast -d (N_ "Small block size")
complete -c bzip2 -s 9 -l best -d (N_ "Large block size")
