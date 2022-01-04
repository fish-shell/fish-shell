complete -c command -n __fish_is_first_token -s h -l help -d 'Display help and exit'
complete -c command -n __fish_is_first_token -s a -l all -d 'Print all external commands by the given name'
complete -c command -n __fish_is_first_token -s q -l quiet -l query -d 'Do not print anything, only set exit status'
complete -c command -n __fish_is_first_token -s s -l search -d 'Print the file that would be executed'
complete -c command -xa "(__fish_complete_subcommand)"
