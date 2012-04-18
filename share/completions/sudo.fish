#
# Completion for sudo
#

complete -c sudo -s h -n "__fish_no_arguments" --description "Display help and exit"
complete -c sudo -s v -n "__fish_no_arguments" --description "Validate"
complete -c sudo -s u -a "(__fish_complete_users)" -x -d "Run command as user"
complete -c sudo -s g -a "(__fish_complete_groups)" -x -d "Run command as group"
complete -c sudo -s E -d "Preserve environment"
complete -c sudo -s P -d "Preserve group vector"
complete -c sudo -s S -d "Read password from stdin"
complete -c sudo -s H -d "Set home"
complete -c sudo -s e -r -d "Edit"
complete -c sudo --description "Command to run" -x -a "(__fish_complete_subcommand_root -u -g)"

# Since sudo runs subcommands, it can accept any switches
complete -c sudo -u
