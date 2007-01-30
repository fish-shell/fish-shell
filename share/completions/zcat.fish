complete -c zcat -x -a "(
	__fish_complete_suffix .gz
	__fish_complete_suffix .tgz
)
"
complete -c zcat -s f -l force --description "Overwrite"
complete -c zcat -s h -l help --description "Display help and exit"
complete -c zcat -s L -l license --description "Print license"
complete -c zcat -s V -l version --description "Display version and exit"

