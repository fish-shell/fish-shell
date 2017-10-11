# Completions for the binary calculator

complete -c bc -s i -l interactive -d "Force interactive mode"
complete -c bc -s l -l mathlib -d "Define math library"
complete -c bc -s w -l warn -d "Give warnings for extensions to POSIX bc"
complete -c bc -s s -l standard -d "Process exactly POSIX bc"
complete -c bc -s q -l quiet -d "Do not print the GNU welcome"
complete -c bc -s v -l version -d "Display version and exit"
complete -c bc -s h -l help -d "Display help and exit"
