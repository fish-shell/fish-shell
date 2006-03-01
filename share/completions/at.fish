#at
complete -f -c at -s V -d (N_ "Display version and exit")
complete -f -c at -s q -d (N_ "Use specified queue")
complete -f -c at -s m -d (N_ "Send mail to user")
complete -c at -s f -x -a "(__fish_complete_suffix (commandline -ct) '' 'At job')" -d (N_ "Read job from file")
complete -f -c at -s l -d (N_ "Alias for atq")
complete -f -c at -s d -d (N_ "Alias for atrm")
complete -f -c at -s v -d (N_ "Show the time")
complete -f -c at -s c -d (N_ "Print the jobs listed")

