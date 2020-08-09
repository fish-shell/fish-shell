#
# Completions for sshfs
#
complete -x -c sshfs -d Hostname -a "(__fish_complete_user_at_hosts):"
#
# Mount Points
#
complete -c sshfs -x -a '(__fish_complete_directories (commandline -ct) "Mount point")'
#
# Command options
#
complete -c sshfs -s V -d "Display version and exit"
complete -c sshfs -s p -x -d Port
complete -c sshfs -s C -d Compression
complete -c sshfs -s o -x -d "Mount options"
complete -c sshfs -s d -d "Enable debug"
complete -c sshfs -s f -d "Foreground operation"
complete -c sshfs -s s -d "Disable multi-threaded operation"
complete -c sshfs -s r -d "Mount options"
complete -c sshfs -s h -d "Display help and exit"
