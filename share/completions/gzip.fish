complete -c gzip -s c -l stdout --description "Compress to stdout"
complete -c gzip -s d -l decompress -x -a "
(
	__fish_complete_suffix .gz
	__fish_complete_suffix .tgz
)
"

complete -c gzip -s f -l force --description "Overwrite"
complete -c gzip -s h -l help --description "Display help and exit"
complete -c gzip -s l -l list --description "List compression information"
complete -c gzip -s L -l license --description "Print license"
complete -c gzip -s n -l no-name --description "Do not save/restore filename"
complete -c gzip -s N -l name --description "Save/restore filename"
complete -c gzip -s q -l quiet --description "Suppress warnings"
complete -c gzip -s r -l recursive --description "Recurse directories"
complete -c gzip -s S -l suffix -r --description "Suffix"
complete -c gzip -s t -l test --description "Check integrity"
complete -c gzip -s v -l verbose --description "Display compression ratios"
complete -c gzip -s V -l version --description "Display version and exit"
complete -c gzip -s 1 -l fast --description "Use fast setting"
complete -c gzip -s 9 -l best --description "Use high compression setting"

