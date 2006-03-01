#
# Completion for sudo
#

complete -c sudo -d (N_ "Command to run") -x -a "(__fish_complete_subcommand)"

complete -c sudo -s h -n "__fish_no_arguments" -d (N_ "Display help and exit")
complete -c sudo -s v -n "__fish_no_arguments" -d (N_ "Validate")
