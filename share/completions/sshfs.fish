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
# Mount Points, for neatness, I am only mounting under ~/mnt/
#
complete -c sshfs --description "Mount point" -x -a '(find ~/mnt -type d)'
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

