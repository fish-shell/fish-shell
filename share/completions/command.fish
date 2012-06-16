
complete -c command -s h -l help --description 'Display help and exit'
complete -c command --description "Command to run" -xa "(__fish_complete_subcommand)"
