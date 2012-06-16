#
# Completions for the umount command
#

#
# Find all mountpoints
#
complete -c umount --description "Mount point" -x -a '(__fish_print_mounted)'

complete -c umount -s V --description "Display version and exit"
complete -c umount -s h --description "Display help and exit"
complete -c umount -s v --description "Verbose mode"
complete -c umount -s n --description "Unmount without writing in /etc/mtab"
complete -c umount -s r --description "In case unmounting fails, try to remount read-only"
complete -c umount -s d --description "In case the unmounted device was a loop device, also free this loop device"
complete -c umount -s i --description "Don't call the /sbin/umount.<filesystem> helper even if it exists"
complete -c umount -s a --description "Unmount all of the file systems described in /etc/mtab"
complete -c umount -s t --description "Actions should only be taken on file systems of the specified type" -xa "(__fish_print_filesystems)"
complete -c umount -s O --description "Actions should only be taken on file systems with the specified options in /etc/fstab" -xa '(cat /etc/mtab | cut -d " " -f 4)\t"Mount option"'
complete -c umount -s f --description "Force unmount (in case of an unreachable NFS system)"
complete -c umount -s l --description "Detach the filesystem from the filesystem hierarchy now, and cleanup all references to the filesystem as soon as it is not busy"

