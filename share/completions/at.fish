#at
complete -f -c at -s V -d (_ "Display version and exit")
complete -f -c at -s q -d (_ "Use specified queue")
complete -f -c at -s m -d (_ "Send mail to user")
complete -c at -s f -x -a "(__fish_complete_suffix (commandline -ct) '' 'At job')" -d (_ "Read job from file")
complete -f -c at -s l -d (_ "Alias for atq")
complete -f -c at -s d -d (_ "Alias for atrm")
complete -f -c at -s v -d (_ "Show the time")
complete -f -c at -s c -d (_ "Print the jobs listed")

