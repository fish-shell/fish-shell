#
# Completions for fusermount
#
# Find mount points, borrowed from umount.fish
#
complete -c fusermount -d "Mount point" -x -a '(__fish_print_mounted)'
complete -c fusermount -s h -d "Display help and exit"
complete -c fusermount -s v -d "Display version and exit"
complete -c fusermount -s o -x -d "Mount options"
complete -c fusermount -s u -d Unmount
complete -c fusermount -s q -d Quiet
complete -c fusermount -s z -d "Lazy unmount"
