complete -c builtin -n 'test (count (commandline -opc)) -eq 1' -s h -l help -d 'Display help and exit'
complete -c builtin -n 'test (count (commandline -opc)) -eq 1' -s n -l names -d 'Print names of all existing builtins'
complete -c builtin -n 'test (count (commandline -opc)) -eq 1' -xa '(builtin -n)'
complete -c builtin -n 'test (count (commandline -opc)) -ge 2' -xa '(__fish_complete_subcommand)'
