#Completions for rm
complete -c rm -s d -l directory --description "Unlink directory (Only by superuser)"
complete -c rm -s f -l force --description "Never prompt before removal"
complete -c rm -s i -l interactive --description "Prompt before removal"
complete -c rm -s r -l recursive --description "Recursively remove subdirectories"
complete -c rm -s R --description "Recursively remove subdirectories"
complete -c rm -s v -l verbose --description "Explain what is done"
complete -c rm -s h -l help --description "Display help and exit"
complete -c rm -l version --description "Display version and exit"


