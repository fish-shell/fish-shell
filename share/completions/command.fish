
complete -c command -s h -l help --description 'Display help and exit'
complete -c command -s s -l search --description 'Print the file that would be executed'
complete -c command --description "Command to run" -xa "(__fish_complete_subcommand)"
