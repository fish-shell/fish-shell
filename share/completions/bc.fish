# Completions for the binary calculator

complete -c bc -s i -l interactive -d (_ "Force interactive mode")
complete -c bc -s l -l math-lib -d (_ "Define math library")
complete -c bc -s w -l warn -d (_ "Give warnings for extensions to POSIX bc")
complete -c bc -s s -l standard -d (_ "Process exactly POSIX bc")
complete -c bc -s q -l quiet -d (_ "Do not print the GNU welcome")
complete -c bc -s v -l version -d (_ "Display version and exit")
complete -c bc -s h -l help -d (_ "Display help and exit")
