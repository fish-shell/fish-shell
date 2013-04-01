complete -c gunzip -s c -l stdout --description "Compress to stdout"
complete -c gunzip -x -a "(
	__fish_complete_suffix .gz
	__fish_complete_suffix .tgz
)
"
complete -c gunzip -s f -l force --description "Overwrite"
complete -c gunzip -s h -l help --description "Display help and exit"
complete -c gunzip -s l -l list --description "List compression information"
complete -c gunzip -s L -l license --description "Print license"
complete -c gunzip -s n -l no-name --description "Do not save/restore filename"
complete -c gunzip -s N -l name --description "Save/restore filename"
complete -c gunzip -s q -l quiet --description "Suppress warnings"
complete -c gunzip -s r -l recursive --description "Recurse directories"
complete -c gunzip -s S -l suffix -r --description "Suffix"
complete -c gunzip -s t -l test --description "Check integrity"
complete -c gunzip -s v -l verbose --description "Display compression ratios"
complete -c gunzip -s V -l version --description "Display version and exit"

