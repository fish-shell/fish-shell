# Completions for the binary calculator

complete -c bc -s i -l interactive -d (N_ "Force interactive mode")
complete -c bc -s l -l math-lib -d (N_ "Define math library")
complete -c bc -s w -l warn -d (N_ "Give warnings for extensions to POSIX bc")
complete -c bc -s s -l standard -d (N_ "Process exactly POSIX bc")
complete -c bc -s q -l quiet -d (N_ "Do not print the GNU welcome")
complete -c bc -s v -l version -d (N_ "Display version and exit")
complete -c bc -s h -l help -d (N_ "Display help and exit")
