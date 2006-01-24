#
# Completion for sudo
#
# Generate a list of commands
#

complete -c sudo -d (_ "Command to run") -x -a "(__fish_complete_commands)"

complete -c sudo -s h -d (_ "Display help and exit")
complete -c sudo -s v -d (_ "Validate")
