
complete -c command -s h -l help -d 'Display help and exit'
complete -c command -s s -l search -d 'Print the file that would be executed'
complete -c command -d "Command to run" -xa "(__fish_complete_subcommand)"
