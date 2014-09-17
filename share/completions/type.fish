
complete -c type -s h -l help --description "Display help and exit"
complete -c type -s a -l all --description "Print all possible definitions of the specified name"
complete -c type -s f -l no-functions --description "Suppress function and builtin lookup"
complete -c type -s t -l type --description "Print command type"
complete -c type -s p -l path --description "Print path to command, or nothing if name is not a command"
complete -c type -s P -l force-path --description "Print path to command"
complete -c type -s q -l quiet --description "Suppress output"

complete -c type -a "(builtin -n)" --description "Builtin"
complete -c type -a "(functions -n)" --description "Function"

complete -c type -a "(complete -C(commandline -ct))" -x
