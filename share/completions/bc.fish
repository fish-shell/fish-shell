# Completions for the binary calculator

complete -c bc -s i -l interactive --description "Force interactive mode"
complete -c bc -s l -l math-lib --description "Define math library"
complete -c bc -s w -l warn --description "Give warnings for extensions to POSIX bc"
complete -c bc -s s -l standard --description "Process exactly POSIX bc"
complete -c bc -s q -l quiet --description "Do not print the GNU welcome"
complete -c bc -s v -l version --description "Display version and exit"
complete -c bc -s h -l help --description "Display help and exit"
