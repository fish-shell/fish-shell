# Completions for top
complete -c top -s b -d (N_ "Batch mode")
complete -c top -s c -d (N_ "Toggle command line/program name")
complete -c top -s d -d (N_ "Update interval") -x
complete -c top -s h -d (N_ "Display help and exit")
complete -c top -s i -d (N_ "Toggle idle processes")
complete -c top -s n -d (N_ "Maximium iterations") -x
complete -c top -s u -d (N_ "Monitor effective UID") -x -a "(__fish_complete_users)"
complete -c top -s U -d (N_ "Monitor user") -x -a "(__fish_complete_users)"
complete -c top -s p -d (N_ "Monitor PID") -x -a "(__fish_complete_pids)"
complete -c top -s s -d (N_ "Secure mode")
complete -c top -s S -d (N_ "Cumulative mode")
complete -c top -s v -d (N_ "Display version and exit")

