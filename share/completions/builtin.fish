complete -c builtin -n __fish_is_first_token -s h -l help -d 'Display help and exit'
complete -c builtin -n __fish_is_first_token -s n -l names -d 'Print names of all existing builtins'
complete -c builtin -n __fish_is_first_token -xa '(builtin -n)'
complete -c builtin -n 'test (count (commandline -opc)) -ge 2' -xa '(__fish_complete_subcommand)'
