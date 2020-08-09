# Completions for top
complete -c top -s b -d "Batch mode"
complete -c top -s c -d "Toggle command line/program name"
complete -c top -s d -d "Update interval" -x
complete -c top -s h -d "Display help and exit"
complete -c top -s i -d "Toggle idle processes"
complete -c top -s n -d "Maximum iterations" -x
complete -c top -s u -d "Monitor effective UID" -x -a "(__fish_complete_users)"
complete -c top -s U -d "Monitor user" -x -a "(__fish_complete_users)"
complete -c top -s p -d "Monitor PID" -x -a "(__fish_complete_pids)"
complete -c top -s s -d "Secure mode"
complete -c top -s S -d "Cumulative mode"
complete -c top -s v -d "Display version and exit"
