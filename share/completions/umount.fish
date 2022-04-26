#
# Completions for the umount command
#

#
# Find all mountpoints
#
complete -c umount -d "Mount point" -x -a '(__fish_print_mounted)'

complete -c umount -s V -l version -d "Display version"
complete -c umount -s h -l help -d "Display help"
complete -c umount -s v -l verbose -d "Say what is being done"
complete -c umount -s n -l no-mtab -d "Don't write to /etc/mtab"
complete -c umount -s r -l read-only -d "In case unmounting fails, try to remount read-only"
complete -c umount -s d -l detach-loop -d "In case the unmounted device was a loop device, also free this loop device"
complete -c umount -s i -l internal-only -d "Don't call the umount.<type> helpers"
complete -c umount -s a -l all -d "Unmount all of the file systems described in /etc/mtab"
complete -c umount -s t -l types -d "Actions should only be taken on file systems of the specified type" -xa "(__fish_print_filesystems)"
complete -c umount -s O -l test-opts -d "Actions should only be taken on file systems with the specified options in /etc/fstab" -xa '(cut -d " " -f 4 /etc/mtab)\t"Mount option"'
complete -c umount -s f -l force -d "Force unmount (in case of an unreachable NFS system)"
complete -c umount -s l -l lazy -d "Lazy unmount (unmount a 'busy' filesystem)"
complete -c umount -s R -l recursive -d "Recursively unmount a target with all its children"
complete -c umount -s A -l all-targets -d "Unmount all mountpoints for the given device in the current namespaces"
complete -c umount -s c -l no-canonicalize -d "Don't canonicalize paths"
complete -c umount -l fake -d "dry run; skip the umount(2) syscall"
