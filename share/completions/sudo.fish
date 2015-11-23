#
# Completion for sudo
#

# All these options should be valid for GNU and OSX sudo
complete -c sudo -s A -d "Ask for password via the askpass or \$SSH_ASKPASS program"
complete -c sudo -s C -d "Close all file descriptors greater or equal to the given number" -a "(seq 0 255)"
complete -c sudo -s E -d "Preserve environment"
complete -c sudo -s H -d "Set home"
complete -c sudo -s K -d "Remove the credential timestamp entirely"
complete -c sudo -s P -d "Preserve group vector"
complete -c sudo -s S -d "Read password from stdin"
complete -c sudo -s b -d "Run command in the background"
complete -c sudo -s e -r -d "Edit"
complete -c sudo -s g -a "(__fish_complete_groups)" -x -d "Run command as group"
complete -c sudo -s h -n "__fish_no_arguments" -d "Display help and exit"
complete -c sudo -s i -d "Run a login shell"
complete -c sudo -s k -d "Reset or ignore the credential timestamp"
complete -c sudo -s l -d "List the allowed and forbidden commands for the given user, or the full path to the given command if it is allowed"
complete -c sudo -s n -d "Do not prompt for a password - if one is needed, fail"
complete -c sudo -s p -d "Specify a custom password prompt"
complete -c sudo -s s -d "Run the given command in a shell"
complete -c sudo -s u -a "(__fish_complete_users)" -x -d "Run command as user"
complete -c sudo -s v -n "__fish_no_arguments" -d "Validate the credentials, extending timeout"

complete -c sudo -d "Command to run" -x -a "(__fish_complete_subcommand_root -u -g)"
# Since sudo runs subcommands, it can accept any switches
complete -c sudo -u
