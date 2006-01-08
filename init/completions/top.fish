# Completions for top
complete -c top -s b -d (_ "Batch mode")
complete -c top -s c -d (_ "Toggle command line/program name")
complete -c top -s d -d (_ "Update interval") -x
complete -c top -s h -d (_ "Display help and exit")
complete -c top -s i -d (_ "Toggle idle processes")
complete -c top -s n -d (_ "Maximium iterations") -x
complete -c top -s u -d (_ "Monitor effective UID") -x -a "(__fish_complete_users)"
complete -c top -s U -d (_ "Monitor user") -x -a "(__fish_complete_users)"
complete -c top -s p -d (_ "Monitor PID") -x -a "(__fish_complete_pids)"
complete -c top -s s -d (_ "Secure mode")
complete -c top -s S -d (_ "Cumulative mode")
complete -c top -s v -d (_ "Display version and exit")

