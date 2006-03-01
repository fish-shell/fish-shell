complete -c gzip -s c -l stdout -d (N_ "Compress to stdout")
complete -c gzip -s d -l decompress -x -a "
(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"

complete -c gzip -s f -l force -d (N_ "Overwrite")
complete -c gzip -s h -l help -d (N_ "Display help and exit")
complete -c gzip -s l -l list -d (N_ "List compression information")
complete -c gzip -s L -l license -d (N_ "Print license")
complete -c gzip -s n -l no-name -d (N_ "Do not save/restore filename")
complete -c gzip -s N -l name -d (N_ "Save/restore filename")
complete -c gzip -s q -l quiet -d (N_ "Supress warnings")
complete -c gzip -s r -l recursive -d (N_ "Recurse directories")
complete -c gzip -s S -l suffix -r -d (N_ "Suffix")
complete -c gzip -s t -l test -d (N_ "Check integrity")
complete -c gzip -s v -l verbose -d (N_ "Display compression ratios")
complete -c gzip -s V -l version -d (N_ "Display version and exit")
complete -c gzip -s 1 -l fast -d (N_ "Use fast setting")
complete -c gzip -s 9 -l best -d (N_ "Use high compression setting")

