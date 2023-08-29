#at
complete -f -c at -s V -d "Display version and exit"
complete -f -c at -s q -d "Use specified queue"
complete -f -c at -s m -d "Send mail to user"
complete -c at -s f -k -x -a "(__fish_complete_suffix  --description='At job' '')" -d "Read job from file"
complete -f -c at -s l -d "Alias for atq"
complete -f -c at -s d -d "Alias for atrm"
complete -f -c at -s v -d "Show the time"
complete -f -c at -s c -d "Print the jobs listed"
