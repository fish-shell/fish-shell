complete -c zcat -x -a "(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"
complete -c zcat -s f -l force -d (_ "Overwrite")
complete -c zcat -s h -l help -d (_ "Display help and exit")
complete -c zcat -s L -l license -d (_ "Print license")
complete -c zcat -s V -l version -d (_ "Display version and exit")

