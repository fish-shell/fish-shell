complete -c gzip -s c -l stdout -d (_ "Compress to stdout")
complete -c gzip -s d -l decompress -x -a "
(
	__fish_complete_suffix (commandline -ct) .gz 'Compressed file'
	__fish_complete_suffix (commandline -ct) .tgz 'Compressed archive'
)
"

complete -c gzip -s f -l force -d (_ "Overwrite")
complete -c gzip -s h -l help -d (_ "Display help and exit")
complete -c gzip -s l -l list -d (_ "List compression information")
complete -c gzip -s L -l license -d (_ "Print license")
complete -c gzip -s n -l no-name -d (_ "Do not save/restore filename")
complete -c gzip -s N -l name -d (_ "Save/restore filename")
complete -c gzip -s q -l quiet -d (_ "Supress warnings")
complete -c gzip -s r -l recursive -d (_ "Recurse directories")
complete -c gzip -s S -l suffix -r -d (_ "Suffix")
complete -c gzip -s t -l test -d (_ "Check integrity")
complete -c gzip -s v -l verbose -d (_ "Display compression ratios")
complete -c gzip -s V -l version -d (_ "Display version and exit")
complete -c gzip -s 1 -l fast -d (_ "Use fast setting")
complete -c gzip -s 9 -l best -d (_ "Use high compression setting")

