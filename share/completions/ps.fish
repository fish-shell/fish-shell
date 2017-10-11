# Completions for ps

complete -c ps -s A -d "Select all"
complete -c ps -s N -d "Invert selection"
complete -c ps -s a -d "Select all processes except session leaders and terminal-less"
complete -c ps -s d -d "Select all processes except session leaders"
complete -c ps -s e -d "Select all"
complete -c ps -l deselect -d "Deselect all processes that do not fulfill conditions"

complete -c ps -s C -d "Select by command" -ra '(__fish_complete_list , __fish_complete_proc)'
complete -c ps -s G -l Group -d "Select by group" -x -a "(__fish_complete_list , __fish_complete_groups)"

complete -c ps -s U -l User -d "Select by user" -x -a "(__fish_complete_list , __fish_complete_users)"
complete -c ps -s u -l user -d "Select by user" -x -a "(__fish_complete_list , __fish_complete_users)"
complete -c ps -s g -l group -d "Select by group/session" -x -a "(__fish_complete_list , __fish_complete_groups)"
complete -c ps -s p -l pid -d "Select by PID" -x -a "(__fish_complete_list , __fish_complete_pids)"
complete -c ps -l ppid -d "Select by parent PID" -x -a "(__fish_complete_list , __fish_complete_pids)"
complete -c ps -s s -l sid -d "Select by session ID" -x -a "(__fish_complete_list , __fish_complete_pids)"
complete -c ps -s t -l tty -d "Select by tty" -r
complete -c ps -s F -d "Extra full format"
complete -c ps -s O -d "User defined format" -x
complete -c ps -s M -d "Add column for security data"
complete -c ps -s c -d 'Show different scheduler information for the -l option'
complete -c ps -s f -d "Full format"
complete -c ps -s j -d "Jobs format"
complete -c ps -s l -d "Long format"
complete -c ps -s o -l format -d "User defined format" -x
complete -c ps -s y -d "Do not show flags"
complete -c ps -s Z -l context -d "Add column for security data"
complete -c ps -s H -l forest -d "Show hierarchy"
complete -c ps -s n -d "Set namelist file" -r
complete -c ps -s w -d "Wide output"
complete -c ps -l cols -l columns -l width -d 'Set screen width' -r
complete -c ps -l cumulative -d 'Include dead child process data'
complete -c ps -l headers -d 'Repead header lines, one per page'
complete -c ps -l no-headers -d 'Print no headers'
complete -c ps -l lines -l rows -d 'Set screen height' -r
complete -c ps -l sort -d 'Spericy sorting order' -r
complete -c ps -s L -d "Show threads. With LWP/NLWP"
complete -c ps -s T -d "Show threads. With SPID"
complete -c ps -s m -d 'Show threads after processes'
complete -c ps -s V -l version -d "Display version and exit"
complete -c ps -l help -d "Display help and exit"
complete -c ps -l info -d "Display debug info"

