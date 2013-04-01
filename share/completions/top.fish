# Completions for top
complete -c top -s b --description "Batch mode"
complete -c top -s c --description "Toggle command line/program name"
complete -c top -s d --description "Update interval" -x
complete -c top -s h --description "Display help and exit"
complete -c top -s i --description "Toggle idle processes"
complete -c top -s n --description "Maximum iterations" -x
complete -c top -s u --description "Monitor effective UID" -x -a "(__fish_complete_users)"
complete -c top -s U --description "Monitor user" -x -a "(__fish_complete_users)"
complete -c top -s p --description "Monitor PID" -x -a "(__fish_complete_pids)"
complete -c top -s s --description "Secure mode"
complete -c top -s S --description "Cumulative mode"
complete -c top -s v --description "Display version and exit"

