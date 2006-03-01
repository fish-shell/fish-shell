#
# Completions for the umount command
#

#
# Find all mountpoints
#
complete -c umount -d (N_ "Mount point") -x -a '(cat /etc/mtab | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"|grep "^/")'

complete -c umount -s V -d (N_ "Display version and exit")
complete -c umount -s h -d (N_ "Display help and exit")
complete -c umount -s v -d (N_ "Verbose mode")
complete -c umount -s n -d (N_ "Unmount without writing in /etc/mtab")
complete -c umount -s r -d (N_ "In case unmounting fails, try to remount read-only")
complete -c umount -s d -d (N_ "In case the unmounted device was a loop device, also free this loop device")
complete -c umount -s i -d (N_ "Don't call the /sbin/umount.<filesystem> helper even if it exists")
complete -c umount -s a -d (N_ "Unmount all of the file systems described in /etc/mtab")
complete -c umount -s t -d (N_ "Actions should only be taken on file systems of the specified type") -xa "(__fish_print_filesystems)"
complete -c umount -s O -d (N_ "Actions should only be taken on file systems with the specified options in /etc/fstab") -xa '(cat /etc/mtab | cut -d " " -f 4)\t"Mount option"'
complete -c umount -s f -d (N_ "Force unmount (in case of an unreachable NFS system)")
complete -c umount -s l -d (N_ "Detach the filesystem from the filesystem hierarchy now, and cleanup all references to the filesystem as soon as it is not busy")

