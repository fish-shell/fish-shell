#Completions for rmdir
complete -x -c rmdir -a "(__fish_complete_directory (commandline -ct))"
complete -c rmdir -l ignore-fail-on-non-empty -d "Ignore errors from non-empty directories"
complete -c rmdir -s p -l parents -d "Remove each component of path"
complete -c rmdir -s v -l verbose -d "Explain what is done"
complete -c rmdir -l help -d "Display help and exit"
complete -c rmdir -l version -d "Display version and exit"

