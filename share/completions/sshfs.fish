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
complete -c sshfs -d (_ "Mount point") -x -a '(find ~/mnt -type d)'
#
# Command options
#
complete -c sshfs -s V -d (_ "Display version and exit")
complete -c sshfs -s p -x -d (_ "Port")
complete -c sshfs -s C -d (_ "Compression")
complete -c sshfs -s o -x -d (_ "Mount options")
complete -c sshfs -s d -d (_ "Enable debug")
complete -c sshfs -s f -d (_ "Foreground operation")
complete -c sshfs -s s -d (_ "Disable multi-threaded operation")
complete -c sshfs -s r -d (_ "Mount options")
complete -c sshfs -s h -d (_ "Display help and exit")

