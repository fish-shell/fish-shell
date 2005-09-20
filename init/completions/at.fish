#at
complete -f -c at -s V -d "print version"
complete -f -c at -s q -d "use specified queue"
complete -f -c at -s m -d "send mail to user"
complete -c at -s f -x -a "(__fish_complete_suffix (commandline -ct) '' 'At job')" -d "Read job from file"
complete -f -c at -s l -d "alias for atq"
complete -f -c at -s d -d "alias for atrm"
complete -f -c at -s v -d "show the time"
complete -f -c at -s c -d "cat the jobs listed"

