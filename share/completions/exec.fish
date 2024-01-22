complete -c exec -n 'test (count (commandline -xpc)) -eq 1' -s h -l help -d 'Display help and exit'
complete -c exec -xa "(__fish_complete_subcommand)"
