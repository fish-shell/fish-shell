# Completions for mount

complete -c mount -a '(__fish_complete_blockdevice)'
# In case `mount UUID=` and similar also works
complete -c mount -a "(test -r /etc/fstab; and string replace -r '#.*' '' < /etc/fstab | string match -r '.+' | string replace -r ' (\S*) .*' '\tMount point \$1' | string replace -a '\040' ' ')"
complete -c mount -a "(test -r /etc/fstab; and string replace -r '#.*' '' < /etc/fstab | string match -r '.+' | string replace -r '(\S*) (\S*) .*' '\$2\tDevice \$1' | string replace -a '\040' ' ')"
# In case it doesn't
# complete -x -c mount -a "(test -r /etc/fstab; and string match -r '^/.*' < /etc/fstab | string replace -r ' ([^\s]*) .*' '\tMount point \$1')"
# complete -x -c mount -a "(test -r /etc/fstab; and string match -r '^/.*' < /etc/fstab | string replace -r '(^/[^\s]*) ([^\s]*) .*' '\$2\tDevice \$1')"
complete -c mount -s V -d 'Display version and exit'
complete -c mount -s h -d 'Display help and exit'
complete -c mount -s v -d 'Verbose mode'
complete -c mount -s a -d 'Mount file systems in fstab'
complete -c mount -s F -d 'Fork process for each mount'
complete -c mount -s f -d 'Fake mounting'
complete -c mount -s l -d 'Add label to output'
complete -c mount -s n -d 'Do not write mtab'
complete -c mount -s s -d 'Tolerate sloppy mount options'
complete -c mount -s r -d 'Read only'
complete -c mount -s w -d 'Read/Write mode'
complete -x -c mount -s L -d 'Mount partition with specified label'
complete -x -c mount -s U -d 'Mount partition with specified UID'
complete -c mount -s O -x -d 'Exclude file systems'
complete -c mount -l bind -f -d 'Remount a subtree to a second position'
complete -c mount -l move -f -d 'Move a subtree to a new position'
complete -c mount -x -s t -d 'File system' -a "(__fish_print_filesystems)"
complete -c mount -x -s o -d 'Mount option' -a '(__fish_complete_mount_opts)'
