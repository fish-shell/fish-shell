# Completions for mount

complete -c mount -a '(__fish_complete_blockdevice)'
# In case `mount UUID=` and similar also works
complete -c mount -a "(test -r /etc/fstab; and string replace -r '#.*' '' < /etc/fstab | string match -r '.+' | string replace -r ' (\S*) .*' '\tMount point \$1' | string replace -a '\040' ' ')"
complete -c mount -a "(test -r /etc/fstab; and string replace -r '#.*' '' < /etc/fstab | string match -r '.+' | string replace -r '(\S*) (\S*) .*' '\$2\tDevice \$1' | string replace -a '\040' ' ')"
# In case it doesn't
# complete -x -c mount -a "(test -r /etc/fstab; and string match -r '^/.*' < /etc/fstab | string replace -r ' ([^\s]*) .*' '\tMount point \$1')"
# complete -x -c mount -a "(test -r /etc/fstab; and string match -r '^/.*' < /etc/fstab | string replace -r '(^/[^\s]*) ([^\s]*) .*' '\$2\tDevice \$1')"

complete -c mount -s a -l all -d 'Mount file systems in fstab'
complete -c mount -s B -l bind -f -d 'Remount a subtree to a second position'
complete -c mount -s c -l no-canonicalize -d 'Do not canonicalize any paths or tags during the mount process'
complete -c mount -s F -l fork -d 'Fork process for each mount'
complete -c mount -s f -l fake -d 'Fake mounting'
complete -c mount -s i -l internal-only -d 'Don’t call the /sbin/mount.filesystem helper even if it exists'
complete -c mount -s L -l label -x -d 'Mount partition with specified label'
complete -c mount -s l -l show-labels -d 'Add label to output'
complete -c mount -s M -l move -f -d 'Move a subtree to a new position'
complete -c mount -s m -l mkdir -d 'Allow to make a target directory (mountpoint) if it does not exist yet'
complete -c mount -l map-groups -l map-users -d 'Add the specified user/group mapping to an X-mount.idmap map'
complete -c mount -s n -l no-mtab -d 'Do not write mtab'
complete -c mount -s N -l namespace -r -d 'Perform the mount operation in the mount namespace specified by ns'
complete -c mount -s O -l test-opts -x -d 'Exclude file systems'
complete -c mount -s o -l options -x -a '(__fish_complete_mount_opts)' -d 'Mount option'
complete -c mount -l onlyonce -d 'Check if already mounted'
complete -c mount -l options-mode -a "ignore append prepend replace" -d 'how to combine with options from fstab/mtab'
complete -c mount -l options-source -d 'Source of default options'
complete -c mount -l options-source-force -d 'Use options from fstab/mtab even if both device and dir are specified'
complete -c mount -s R -l rbind -d 'Remount a subtree and all possible submounts somewhere else'
complete -c mount -s r -l read-only -d 'Read only'
complete -c mount -s s -d 'Tolerate sloppy mount options'
complete -c mount -l source -r -d 'explicitly define the argument as the mount source'
complete -c mount -l target -r -d 'explicitly define the argument as the mount target'
complete -c mount -l target-prefix -r -d 'Prepend the specified directory to all mount targets'
complete -c mount -s T -l fstab -r -d 'Specifies an alternative fstab file'
complete -c mount -s t -l types -x -a '(__fish_print_filesystems)' -d 'File system'
complete -c mount -s U -l uuid -x -d 'Mount partition with specified UID'
complete -c mount -s v -l verbose -d 'Verbose mode'
complete -c mount -s w -l rw -l read-write -d 'Read/Write mode'
complete -c mount -s h -l help -d 'Display help and exit'
complete -c mount -s V -l version -d 'Display version and exit'
