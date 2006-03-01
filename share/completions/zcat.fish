complete -c zcat -x -a "(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"
complete -c zcat -s f -l force -d (N_ "Overwrite")
complete -c zcat -s h -l help -d (N_ "Display help and exit")
complete -c zcat -s L -l license -d (N_ "Print license")
complete -c zcat -s V -l version -d (N_ "Display version and exit")

