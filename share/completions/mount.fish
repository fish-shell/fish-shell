

# A list of all known filesystem types, used by various completions,
# including mount and df

# Completions for mount
complete -x -c mount -a '(cat /etc/fstab|sed -e "s/^\([^ \t]*\)[ \t]*\([^ \t]*\).*/\1\n\2/"|grep "^/")' -d (N_ 'Mount point')
complete -c mount -s V -d (N_ 'Display version and exit')
complete -c mount -s h -d (N_ 'Display help and exit')
complete -c mount -s v -d (N_ 'Verbose mode')
complete -c mount -s a -d (N_ 'Mount filesystems in fstab')
complete -c mount -s F -d (N_ 'Fork process for each mount')
complete -c mount -s f -d (N_ 'Fake mounting')
complete -c mount -s l -d (N_ 'Add label to output')
complete -c mount -s n -d (N_ 'Do not write mtab')
complete -c mount -s s -d (N_ 'Tolerate sloppy mount options')
complete -c mount -s r -d (N_ 'Read only')
complete -c mount -s w -d (N_ 'Read/Write mode')
complete -x -c mount -s L -d (N_ 'Mount partition with specified label')
complete -x -c mount -s U -d (N_ 'Mount partition with specified UID')
complete -c mount -s O -x -d (N_ 'Exclude filesystems')
complete -c mount -l bind -f -d (N_ 'Remount a subtree to a second position')
complete -c mount -l move -f -d (N_ 'Move a subtree to a new position')
complete -c mount -x -s t -d (N_ 'Filesystem') -a "(__fish_print_filesystems)"

complete -c mount -x -s o -d (N_ 'Mount option') -a '(__fish_append , $__fish_mount_opts)'

set -g __fish_mount_opts async\tUse\ asynchronous\ I/O atime\tUpdate\ time\ on\ each\ access auto\tMounted\ with\ -a defaults\tUse\ default\ options dev\tInterpret\ character/block\ special\ devices exec\tPermit\ executables _netdev\tFilesystem\ uses\ network noatime\tDo\ not\ update\ time\ on\ each\ access noauto\tNot\ mounted\ by\ -a nodev\tDo\ not\ interpret\ character/block\ special\ devices noexec\tDo\ not\ permit\ executables nosuid\tIgnore\ suid\ bits nouser\tOnly\ root\ may\ mount remount\tRemount\ read-only\ filesystem ro\tMount\ read-only rw\tMount\ read-write suid\tAllow\ suid\ bits sync\tUse\ synchronous\ I/O dirsync\tUse\ synchronous\ directory\ operations user\tAny\ user\ may\ mount users\tAny\ user\ may\ mount\ and\ unmount


