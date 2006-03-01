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
complete -c sshfs -d (N_ "Mount point") -x -a '(find ~/mnt -type d)'
#
# Command options
#
complete -c sshfs -s V -d (N_ "Display version and exit")
complete -c sshfs -s p -x -d (N_ "Port")
complete -c sshfs -s C -d (N_ "Compression")
complete -c sshfs -s o -x -d (N_ "Mount options")
complete -c sshfs -s d -d (N_ "Enable debug")
complete -c sshfs -s f -d (N_ "Foreground operation")
complete -c sshfs -s s -d (N_ "Disable multi-threaded operation")
complete -c sshfs -s r -d (N_ "Mount options")
complete -c sshfs -s h -d (N_ "Display help and exit")

