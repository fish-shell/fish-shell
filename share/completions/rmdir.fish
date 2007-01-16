#Completions for rmdir
complete -x -c rmdir -a "(__fish_complete_directories (commandline -ct))"
complete -c rmdir -l ignore-fail-on-non-empty --description "Ignore errors from non-empty directories"
complete -c rmdir -s p -l parents --description "Remove each component of path"
complete -c rmdir -s v -l verbose --description "Verbose mode"
complete -c rmdir -l help --description "Display help and exit"
complete -c rmdir -l version --description "Display version and exit"

