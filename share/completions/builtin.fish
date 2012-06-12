
complete -c builtin -s h -l help --description 'Display help and exit'
complete -c builtin -s n -l names --description 'Print names of all existing builtins'
complete -c builtin -xa '(builtin -n)'
complete -c builtin -n '__fish_use_subcommand' -xa '__fish_complete_subcommand'
