complete -c type -s h -l help -d "Display help and exit"
complete -c type -s a -l all -d "Print all possible definitions of the specified name"
complete -c type -s f -l no-functions -d "Suppress function and builtin lookup"
complete -c type -s t -l type -d "Print command type"
complete -c type -s p -l path -d "Print path to command, or nothing if name is not a command"
complete -c type -s P -l force-path -d "Print path to command"
complete -c type -s q -l query -l quiet -d "Check if something exists without output"

complete -c type -a "(builtin -n)" -d Builtin
complete -c type -a "(functions -n)" -d Function

complete -c type -a "(__fish_complete_command)" -x
