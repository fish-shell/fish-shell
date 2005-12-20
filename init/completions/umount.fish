#
# Completions for the umount command
#

#
# Depends on mount for $__fish_filesystems
#
complete -y mount

#
# Find all mountpoints
#
complete -c umount -d "Mount point" -x -a '(cat /etc/mtab | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"|grep "^/")'

complete -c umount -s V -d "Display version and exit"
complete -c umount -s h -d "Display help and exit"
complete -c umount -s v -d "Verbose mode"
complete -c umount -s n -d "Unmount without writing in /etc/mtab"
complete -c umount -s r -d "In case unmounting fails, try to remount read-only"
complete -c umount -s d -d "In case the unmounted device was a loop device, also free this loop device"
complete -c umount -s i -d "Don"\'"t call the /sbin/umount.<filesystem> helper even if it exists"
complete -c umount -s a -d "All of the file systems described in /etc/mtab are unmounted"
complete -c umount -s t -d "Actions should only be taken on file systems of the specified type" -xa $__fish_filesystems
complete -c umount -s O -d "Indicate  that  the  actions should only be taken on file systems with the specified options in /etc/fstab" -xa '(cat /etc/mtab | cut -d " " -f 4)\t"Mount option"'
complete -c umount -s f -d "Force unmount (in case of an unreachable NFS system)"
complete -c umount -s l -d "Detach the filesystem from the filesystem hierarchy now, and cleanup all references to the filesystem as soon as it is not busy"

