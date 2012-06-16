# Completions for ps

complete -c ps -s A --description "Select all"
complete -c ps -s N --description "Invert selection"
complete -c ps -s a --description "Select all processes except session leaders and terminal-less"
complete -c ps -s d --description "Select all processes except session leaders"
complete -c ps -s e --description "Select all"
complete -c ps -l deselect --description "Deselect all processes that do not fulfill conditions"

complete -c ps -s C --description "Select by command" -ra '(__fish_complete_list , __fish_complete_proc)'
complete -c ps -s G -l Group --description "Select by group" -x -a "(__fish_complete_list , __fish_complete_groups)"

complete -c ps -s U -l User --description "Select by user" -x -a "(__fish_complete_list , __fish_complete_users)"
complete -c ps -s u -l user --description "Select by user" -x -a "(__fish_complete_list , __fish_complete_users)"
complete -c ps -s g -l group --description "Select by group/session" -x -a "(__fish_complete_list , __fish_complete_groups)"
complete -c ps -s p -l pid --description "Select by PID" -x -a "(__fish_complete_list , __fish_complete_pids)"
complete -c ps -l ppid --description "Select by parent PID" -x -a "(__fish_complete_list , __fish_complete_pids)"
complete -c ps -s s -l sid --description "Select by session ID" -x -a "(__fish_complete_list , __fish_complete_pids)"
complete -c ps -s t -l tty --description "Select by tty" -r
complete -c ps -s F --description "Extra full format"
complete -c ps -s O --description "User defined format" -x
complete -c ps -s M --description "Add column for security data"
complete -c ps -s c -d 'Show different scheduler information for the -l option'
complete -c ps -s f --description "Full format"
complete -c ps -s j --description "Jobs format"
complete -c ps -s l --description "Long format"
complete -c ps -s o -l format --description "User defined format" -x
complete -c ps -s y --description "Do not show flags"
complete -c ps -s Z -l context --description "Add column for security data"
complete -c ps -s H -l forest --description "Show hierarchy"
complete -c ps -s n --description "Set namelist file" -r
complete -c ps -s w --description "Wide output"
complete -c ps -l cols -l columns -l width -d 'Set screen width' -r
complete -c ps -l cumulative -d 'Include dead child process data'
complete -c ps -l headers -d 'Repead header lines, one per page'
complete -c ps -l no-headers -d 'Print no headers'
complete -c ps -l lines -l rows -d 'Set screen height' -r
complete -c ps -l sort -d 'Spericy sorting order' -r
complete -c ps -s L --description "Show threads. With LWP/NLWP"
complete -c ps -s T --description "Show threads. With SPID"
complete -c ps -s m -d 'Show threads after processes'
complete -c ps -s V -l version --description "Display version and exit"
complete -c ps -l help --description "Display help and exit"
complete -c ps -l info --description "Display debug info"

