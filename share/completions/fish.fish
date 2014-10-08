complete -c fish -s c -l "command" --description "Run fish with this command"
complete -c fish -s h -l help --description "Display help and exit"
complete -c fish -s v -l version --description "Display version and exit"
complete -c fish -s n -l no-execute --description "Only parse input, do not execute"
complete -c fish -s i -l interactive --description "Run in interactive mode"
complete -c fish -s l -l login --description "Run in login mode"
complete -c fish -s p -l profile --description "Output profiling information to specified file" -f
complete -c fish -s d -l debug --description "Run with the specified verbosity level"
