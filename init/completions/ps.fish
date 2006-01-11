# Completions for ps

complete -c ps -s A -d (_ "Select all")
complete -c ps -s N -d (_ "Invert selection")
complete -c ps -s a -d (_ "Select all processes except session leaders and terminal-less")
complete -c ps -s d -d (_ "Select all processes except session leaders")
complete -c ps -s e -d (_ "Select all")
complete -c ps -l deselect -d (_ "Deselect all processes that do not fulfill conditions")

complete -c ps -s C -d (_ "Select by command") -r
complete -c ps -s G -l Group -d (_ "Select by group") -x -a "(__fish_complete_groups)"

complete -c ps -s U -l User -d (_ "Select by user") -x -a "(__fish_complete_users)"
complete -c ps -s u -l user -d (_ "Select by user") -x -a "(__fish_complete_users)"
complete -c ps -s g -l group -d (_ "Select by group/session") -x -a "(__fish_complete_groups)"
complete -c ps -s p -l pid -d (_ "Select by PID") -x -a "(__fish_complete_pids)"
complete -c ps -l ppid -d (_ "Select by parent PID") -x -a "(__fish_complete_pids)"
complete -c ps -s s -l sid -d (_ "Select by session ID") -x -a "(__fish_complete_pids)"
complete -c ps -s t -l tty -d (_ "Select by tty") -r
complete -c ps -s F -d (_ "Extra full format")
complete -c ps -s O -d (_ "User defined format") -x
complete -c ps -s M -d (_ "Add column for security data")
complete -c ps -s f -d (_ "Full format")
complete -c ps -s j -d (_ "Jobs format")
complete -c ps -s l -d (_ "Long format")
complete -c ps -s o -l format -d (_ "User defined format") -x
complete -c ps -s y -d (_ "Do not show flags")
complete -c ps -s Z -l context -d (_ "Add column for security data")
complete -c ps -s H -l forest -d (_ "Show hierarchy")
complete -c ps -s n -d (_ "Set namelist file") -r
complete -c ps -s w -d (_ "Wide output")
complete -c ps -s L -d (_ "Show threads")
complete -c ps -s T -d (_ "Show threads")
complete -c ps -s V -l version -d (_ "Display version and exit")
complete -c ps -l help -d (_ "Display help and exit")
complete -c ps -l info -d (_ "Display debug info")

