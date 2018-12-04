# Completions for complete

complete -c complete -s c -l command -d "Command to add completion to" -r
complete -c complete -s p -l path -d "Path to add completion to"
complete -c complete -s s -l short-option -d "POSIX-style short option to complete"
complete -c complete -s l -l long-option -d "GNU-style long option to complete"
complete -c complete -s o -l old-option -d "Old style long option to complete"
complete -c complete -s f -l no-files -d "Don't use file completion"
complete -c complete -s r -l require-parameter -d "Require parameter"
complete -c complete -s x -l exclusive -d "Require parameter and don\'t use file completion"
complete -c complete -s a -l arguments -d "Space-separated list of possible option arguments"
complete -c complete -s d -l description -d "Description of completion"
complete -c complete -s e -l erase -d "Remove completion"
complete -c complete -s h -l help -d "Display help and exit"
complete -c complete -s C -l do-complete -d "Print completions for a commandline specified as a parameter"
complete -c complete -s n -l condition -d "Completion only used if command has zero exit status" -r
complete -c complete -s w -l wraps -d "Inherit completions from specified command"

# Deprecated options

# complete -c complete -s A -l authoritative
# complete -c complete -s u -l unauthoritative
