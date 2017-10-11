complete -c zcat -x -a "(
	__fish_complete_suffix .gz
	__fish_complete_suffix .tgz
)
"
complete -c zcat -s f -l force -d "Overwrite"
complete -c zcat -s h -l help -d "Display help and exit"
complete -c zcat -s L -l license -d "Print license"
complete -c zcat -s V -l version -d "Display version and exit"

