#
# Completions for the umount command
#

#
# Find all mountpoints
#
complete -c umount -d "Mount point" -x -a '(__fish_print_mounted)'

complete -c umount -s V -d "Display version and exit"
complete -c umount -s h -d "Display help and exit"
complete -c umount -s v -d "Verbose mode"
complete -c umount -s n -d "Unmount without writing in /etc/mtab"
complete -c umount -s r -d "In case unmounting fails, try to remount read-only"
complete -c umount -s d -d "In case the unmounted device was a loop device, also free this loop device"
complete -c umount -s i -d "Don't call the /sbin/umount.<filesystem> helper even if it exists"
complete -c umount -s a -d "Unmount all of the file systems described in /etc/mtab"
complete -c umount -s t -d "Actions should only be taken on file systems of the specified type" -xa "(__fish_print_filesystems)"
complete -c umount -s O -d "Actions should only be taken on file systems with the specified options in /etc/fstab" -xa '(cut -d " " -f 4 /etc/mtab)\t"Mount option"'
complete -c umount -s f -d "Force unmount (in case of an unreachable NFS system)"
complete -c umount -s l -d "Detach the filesystem from the filesystem hierarchy now, and cleanup all references to the filesystem as soon as it is not busy"

