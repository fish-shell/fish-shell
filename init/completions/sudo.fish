#
# Completion for sudo
#

complete -c sudo -d (_ "Command to run") -x -a "(__fish_complete_subcommand)"

complete -c sudo -s h -d (_ "Display help and exit")
complete -c sudo -s v -d (_ "Validate")
