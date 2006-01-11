#Completions for rmdir
complete -x -c rmdir -a "(__fish_complete_directory (commandline -ct))"
complete -c rmdir -l ignore-fail-on-non-empty -d (_ "Ignore errors from non-empty directories")
complete -c rmdir -s p -l parents -d (_ "Remove each component of path")
complete -c rmdir -s v -l verbose -d (_ "Verbose mode")
complete -c rmdir -l help -d (_ "Display help and exit")
complete -c rmdir -l version -d (_ "Display version and exit")

