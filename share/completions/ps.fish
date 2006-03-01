# Completions for ps

complete -c ps -s A -d (N_ "Select all")
complete -c ps -s N -d (N_ "Invert selection")
complete -c ps -s a -d (N_ "Select all processes except session leaders and terminal-less")
complete -c ps -s d -d (N_ "Select all processes except session leaders")
complete -c ps -s e -d (N_ "Select all")
complete -c ps -l deselect -d (N_ "Deselect all processes that do not fulfill conditions")

complete -c ps -s C -d (N_ "Select by command") -r
complete -c ps -s G -l Group -d (N_ "Select by group") -x -a "(__fish_complete_groups)"

complete -c ps -s U -l User -d (N_ "Select by user") -x -a "(__fish_complete_users)"
complete -c ps -s u -l user -d (N_ "Select by user") -x -a "(__fish_complete_users)"
complete -c ps -s g -l group -d (N_ "Select by group/session") -x -a "(__fish_complete_groups)"
complete -c ps -s p -l pid -d (N_ "Select by PID") -x -a "(__fish_complete_pids)"
complete -c ps -l ppid -d (N_ "Select by parent PID") -x -a "(__fish_complete_pids)"
complete -c ps -s s -l sid -d (N_ "Select by session ID") -x -a "(__fish_complete_pids)"
complete -c ps -s t -l tty -d (N_ "Select by tty") -r
complete -c ps -s F -d (N_ "Extra full format")
complete -c ps -s O -d (N_ "User defined format") -x
complete -c ps -s M -d (N_ "Add column for security data")
complete -c ps -s f -d (N_ "Full format")
complete -c ps -s j -d (N_ "Jobs format")
complete -c ps -s l -d (N_ "Long format")
complete -c ps -s o -l format -d (N_ "User defined format") -x
complete -c ps -s y -d (N_ "Do not show flags")
complete -c ps -s Z -l context -d (N_ "Add column for security data")
complete -c ps -s H -l forest -d (N_ "Show hierarchy")
complete -c ps -s n -d (N_ "Set namelist file") -r
complete -c ps -s w -d (N_ "Wide output")
complete -c ps -s L -d (N_ "Show threads")
complete -c ps -s T -d (N_ "Show threads")
complete -c ps -s V -l version -d (N_ "Display version and exit")
complete -c ps -l help -d (N_ "Display help and exit")
complete -c ps -l info -d (N_ "Display debug info")

