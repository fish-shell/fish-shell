complete -c bzip2 -s c -l stdout -d "Compress to stdout"
complete -c bzip2 -s d -l decompress -x -a "(
	__fish_complete_suffix (commandline -ct) .bz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .bz2 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tbz 'Compressed archive'
	__fish_complete_suffix (commandline -ct) .tbz2 'Compressed archive'
)
"
complete -c bzip2 -s z -l compress -d "Compress file"
complete -c bzip2 -s t -l test -d "Check integrity"
complete -c bzip2 -s f -l force -d "Overwrite"
complete -c bzip2 -s k -l keep -d "Do not overwrite"
complete -c bzip2 -s s -l small -d "Reduce memory usage"
complete -c bzip2 -s q -l quiet -d "Supress errors"
complete -c bzip2 -s v -l verbose -d "Print compression ratios"
complete -c bzip2 -s L -l license -d "Print license"
complete -c bzip2 -s V -l version -d "Print version"
complete -c bzip2 -s 1 -l fast -d "Small block size"
complete -c bzip2 -s 9 -l best -d "Large block size"
