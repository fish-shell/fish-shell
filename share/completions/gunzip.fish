complete -c gunzip -s c -l stdout -d (N_ "Compress to stdout")
complete -c gunzip -x -a "(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"
complete -c gunzip -s f -l force -d (N_ "Overwrite")
complete -c gunzip -s h -l help -d (N_ "Display help and exit")
complete -c gunzip -s l -l list -d (N_ "List compression information")
complete -c gunzip -s L -l license -d (N_ "Print license")
complete -c gunzip -s n -l no-name -d (N_ "Do not save/restore filename")
complete -c gunzip -s N -l name -d (N_ "Save/restore filename")
complete -c gunzip -s q -l quiet -d (N_ "Supress warnings")
complete -c gunzip -s r -l recursive -d (N_ "Recurse directories")
complete -c gunzip -s S -l suffix -r -d (N_ "Suffix")
complete -c gunzip -s t -l test -d (N_ "Check integrity")
complete -c gunzip -s v -l verbose -d (N_ "Display compression ratios")
complete -c gunzip -s V -l version -d (N_ "Display version and exit")

complete -c gunzip -x -a "(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"

