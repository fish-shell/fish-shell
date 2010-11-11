#at
complete -f -c at -s V --description "Display version and exit"
complete -f -c at -s q --description "Use specified queue"
complete -f -c at -s m --description "Send mail to user"
complete -c at -s f -x -a "(__fish_complete_suffix (commandline -ct) '' 'At job')" --description "Read job from file"
complete -f -c at -s l --description "Alias for atq"
complete -f -c at -s d --description "Alias for atrm"
complete -f -c at -s v --description "Show the time"
complete -f -c at -s c --description "Print the jobs listed"

