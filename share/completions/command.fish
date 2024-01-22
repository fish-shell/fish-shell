complete -c command -n 'test (count (commandline -xpc)) -eq 1' -s h -l help -d 'Display help and exit'
complete -c command -n 'test (count (commandline -xpc)) -eq 1' -s a -l all -d 'Print all external commands by the given name'
complete -c command -n 'test (count (commandline -xpc)) -eq 1' -s q -l quiet -l query -d 'Do not print anything, only set exit status'
complete -c command -n 'test (count (commandline -xpc)) -eq 1' -s s -s v -l search -d 'Print the file that would be executed'
complete -c command -xa "(__fish_complete_subcommand)"
