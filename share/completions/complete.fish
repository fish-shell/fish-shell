# complete.fish

complete -c complete -s c -l command -d "Command to add completion to" -r
complete -c complete -s p -l path -d "Path to add completion to"
complete -c complete -s s -l short-option -d "POSIX-style option to complete"
complete -c complete -s l -l long-option -d "GNU-style option to complete"
complete -c complete -s o -l old-option -d "Old style long option to complete"
complete -c complete -s f -l no-files -d "Do not use file completion"
complete -c complete -s r -l require-parameter -d "Require parameter"
complete -c complete -s x -l exclusive -d "Require parameter and do not use file completion"
complete -c complete -s a -l arguments -d "A list of possible arguments"
complete -c complete -s d -l description -d "Description of this completions"
complete -c complete -s u -l unauthorative -d "Option list is not complete"
complete -c complete -s e -l erase -d "Remove completion"
complete -c complete -s h -l help -d "Display help and exit"
complete -c complete -s C -l do-complete -d "Print all completions for the specified commandline"
complete -c complete -s n -l condition -d "The completion should only be used if the specified command has a zero exit status" -r
complete -c complete -s w -l wraps -d "Inherit completions from the specified command"
