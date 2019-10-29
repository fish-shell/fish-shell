complete -c exec -n 'test (count (commandline -opc)) -eq 1' -s h -l help -d 'Display help and exit'
complete -c exec -n 'test (count (commandline -opc)) -eq 1' -xa "(__fish_complete_external_command)"
complete -c exec -n 'test (count (commandline -opc)) -ge 2' -xa "(__fish_complete_subcommand)"
