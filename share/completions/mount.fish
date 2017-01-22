# Completions for mount

complete -x -c mount -a '(__fish_complete_blockdevice)'
# In case `mount UUID=` and similar also works
complete -x -c mount -a "(test -r /etc/fstab; and string replace -r '#.*' '' < /etc/fstab | string match -r '.+' | string replace -r ' (\S*) .*' '\tMount point \$1' | string replace -a '\040' ' ')"
complete -x -c mount -a "(test -r /etc/fstab; and string replace -r '#.*' '' < /etc/fstab | string match -r '.+' | string replace -r '(\S*) (\S*) .*' '\$2\tDevice \$1' | string replace -a '\040' ' ')"
# In case it doesn't
# complete -x -c mount -a "(test -r /etc/fstab; and string match -r '^/.*' < /etc/fstab | string replace -r ' ([^\s]*) .*' '\tMount point \$1')"
# complete -x -c mount -a "(test -r /etc/fstab; and string match -r '^/.*' < /etc/fstab | string replace -r '(^/[^\s]*) ([^\s]*) .*' '\$2\tDevice \$1')"
complete -c mount -s V --description 'Display version and exit'
complete -c mount -s h --description 'Display help and exit'
complete -c mount -s v --description 'Verbose mode'
complete -c mount -s a --description 'Mount file systems in fstab'
complete -c mount -s F --description 'Fork process for each mount'
complete -c mount -s f --description 'Fake mounting'
complete -c mount -s l --description 'Add label to output'
complete -c mount -s n --description 'Do not write mtab'
complete -c mount -s s --description 'Tolerate sloppy mount options'
complete -c mount -s r --description 'Read only'
complete -c mount -s w --description 'Read/Write mode'
complete -x -c mount -s L --description 'Mount partition with specified label'
complete -x -c mount -s U --description 'Mount partition with specified UID'
complete -c mount -s O -x --description 'Exclude file systems'
complete -c mount -l bind -f --description 'Remount a subtree to a second position'
complete -c mount -l move -f --description 'Move a subtree to a new position'
complete -c mount -x -s t --description 'File system' -a "(__fish_print_filesystems)"
complete -c mount -x -s o --description 'Mount option' -a '(__fish_complete_mount_opts)'
