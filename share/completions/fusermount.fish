#
# Completions for fusermount
#
# Find mount points, borrowed from umount.fish
#
complete -c fusermount -d (N_ "Mount point") -x -a '
(
	cat /etc/mtab | sgrep "^sshfs" | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"|sgrep "^/"
	cat /etc/mtab | sgrep "^fuseiso" | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"|sgrep "^/"
)
'

complete -c fusermount -s h -d (N_ "Display help and exit")
complete -c fusermount -s v -d (N_ "Display version and exit")
complete -c fusermount -s o -x -d (N_ "Mount options")
complete -c fusermount -s u -d (N_ "Unmount")
complete -c fusermount -s q -d (N_ "Quiet")
complete -c fusermount -s z -d (N_ "Lazy unmount")

