#
# Completions for fusermount
#
# Find mount points, borrowed from umount.fish
#
complete -c fusermount --description "Mount point" -x -a '(__fish_print_mounted)'
complete -c fusermount -s h --description "Display help and exit"
complete -c fusermount -s v --description "Display version and exit"
complete -c fusermount -s o -x --description "Mount options"
complete -c fusermount -s u --description "Unmount"
complete -c fusermount -s q --description "Quiet"
complete -c fusermount -s z --description "Lazy unmount"

