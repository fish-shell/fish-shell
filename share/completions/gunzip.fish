complete -c gunzip -s c -l stdout -d (_ "Compress to stdout")
complete -c gunzip -x -a "(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"
complete -c gunzip -s f -l force -d (_ "Overwrite")
complete -c gunzip -s h -l help -d (_ "Display help and exit")
complete -c gunzip -s l -l list -d (_ "List compression information")
complete -c gunzip -s L -l license -d (_ "Print license")
complete -c gunzip -s n -l no-name -d (_ "Do not save/restore filename")
complete -c gunzip -s N -l name -d (_ "Save/restore filename")
complete -c gunzip -s q -l quiet -d (_ "Supress warnings")
complete -c gunzip -s r -l recursive -d (_ "Recurse directories")
complete -c gunzip -s S -l suffix -r -d (_ "Suffix")
complete -c gunzip -s t -l test -d (_ "Check integrity")
complete -c gunzip -s v -l verbose -d (_ "Display compression ratios")
complete -c gunzip -s V -l version -d (_ "Display version and exit")

complete -c gunzip -x -a "(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"

