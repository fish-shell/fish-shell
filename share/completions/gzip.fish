complete -c gzip -s c -l stdout -d "Compress to stdout"
complete -c gzip -s d -l decompress -k -x -a "(__fish_complete_suffix .gz .tgz)"

complete -c gzip -s f -l force -d Overwrite
complete -c gzip -s h -l help -d "Display help and exit"
complete -c gzip -s k -l keep -d "Keep input files"
complete -c gzip -s l -l list -d "List compression information"
complete -c gzip -s L -l license -d "Print license"
complete -c gzip -s n -l no-name -d "Do not save/restore filename"
complete -c gzip -s N -l name -d "Save/restore filename"
complete -c gzip -s q -l quiet -d "Suppress warnings"
complete -c gzip -s r -l recursive -d "Recurse directories"
complete -c gzip -s S -l suffix -r -d Suffix
complete -c gzip -s t -l test -d "Check integrity"
complete -c gzip -s v -l verbose -d "Display compression ratios"
complete -c gzip -s V -l version -d "Display version and exit"
complete -c gzip -s 1 -l fast -d "Use fast setting"
complete -c gzip -s 9 -l best -d "Use high compression setting"
