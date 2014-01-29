#
# Completions for sshfs
#
# Host combinations, borrowed from ssh.fish
#
complete -x -c sshfs -d Hostname -a "

(__fish_print_hostnames):

(
       #Prepend any username specified in the completion to the hostname
       echo (commandline -ct)|sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames):

(__fish_print_users)@
"
#
# Mount Points
#
complete -c sshfs -x -a '(__fish_complete_directories (commandline -ct) "Mount point")'
#
# Command options
#
complete -c sshfs -s V --description "Display version and exit"
complete -c sshfs -s p -x --description "Port"
complete -c sshfs -s C --description "Compression"
complete -c sshfs -s o -x --description "Mount options"
complete -c sshfs -s d --description "Enable debug"
complete -c sshfs -s f --description "Foreground operation"
complete -c sshfs -s s --description "Disable multi-threaded operation"
complete -c sshfs -s r --description "Mount options"
complete -c sshfs -s h --description "Display help and exit"

