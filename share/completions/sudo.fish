#
# Completion for sudo
#

complete -c sudo --description "Command to run" -x -a '(__fish_complete_subcommand_root)'

complete -c sudo -s h -n "__fish_no_arguments" --description "Display help and exit"
complete -c sudo -s v -n "__fish_no_arguments" --description "Validate"

# Since sudo runs subcommands, it can accept any switches
complete -c sudo -u
