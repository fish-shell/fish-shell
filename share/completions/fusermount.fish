#
# Completions for fusermount
#
# Find mount points, borrowed from umount.fish
#
complete -c fusermount --description "Mount point" -x -a '
(
	cat /etc/mtab | __fish_sgrep "^sshfs" | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"| __fish_sgrep "^/"
	cat /etc/mtab | __fish_sgrep "^fuseiso" | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"| __fish_sgrep "^/"
)
'

complete -c fusermount -s h --description "Display help and exit"
complete -c fusermount -s v --description "Display version and exit"
complete -c fusermount -s o -x --description "Mount options"
complete -c fusermount -s u --description "Unmount"
complete -c fusermount -s q --description "Quiet"
complete -c fusermount -s z --description "Lazy unmount"

