#Completions for rmdir
complete -x -c rmdir -a "(__fish_complete_directory (commandline -ct))"
complete -c rmdir -l ignore-fail-on-non-empty -d (N_ "Ignore errors from non-empty directories")
complete -c rmdir -s p -l parents -d (N_ "Remove each component of path")
complete -c rmdir -s v -l verbose -d (N_ "Verbose mode")
complete -c rmdir -l help -d (N_ "Display help and exit")
complete -c rmdir -l version -d (N_ "Display version and exit")

