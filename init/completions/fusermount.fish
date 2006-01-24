#
# Completions for fusermount
#
# Find mount points, borrowed from umount.fish
#
complete -c fusermount -d (_ "Mount point") -x -a '
(
	cat /etc/mtab | grep "^sshfs" | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"|grep "^/"
)
'

complete -c fusermount -s h -d (_ "Display help and exit")
complete -c fusermount -s v -d (_ "Display version and exit")
complete -c fusermount -s o -x -d (_ "Mount options")
complete -c fusermount -s u -d (_ "Unmount")
complete -c fusermount -s q -d (_ "Quiet")
complete -c fusermount -s z -d (_ "Lazy unmount")
