

# A list of all known filesystem types, used by various completions,
# including mount and df

set -g __fish_filesystems "
	adfs
	affs
	autofs
	coda
	coherent
	cramfs
	devpts
	efs
	ext
	ext2
	ext3
	hfs
	hpfs
	iso9660
	jfs
	minix
	msdos
	ncpfs
	nfs
	ntfs
	proc
	qnx4
	ramfs
	reiserfs
	romfs
	smbfs
	sysv
	tmpfs
	udf
	ufs
	umsdos
	vfat
	xenix
	xfs
	xiafs
"

# Completions for mount
complete -x -c mount -a "(cat /etc/fstab|sed -e 's/^\([^ \t]*\)[ \t]*\([^ \t]*\).*/\1\n\2/'|grep '^/')" -d "Mount point"
complete -c mount -s V -d "Display version and exit"
complete -c mount -s h -d "Display help and exit"
complete -c mount -s v -d "Verbose mode"
complete -c mount -s a -d "Mount filesystems in fstab"
complete -c mount -s F -d "Fork process for each mount"
complete -c mount -s f -d "Fake mounting"
complete -c mount -s l -d "Add label to output"
complete -c mount -s n -d "Do not write mtab"
complete -c mount -s s -d "Tolerate sloppy mount options"
complete -c mount -s r -d "Read only"
complete -c mount -s w -d "Read/Write mode"
complete -x -c mount -s L -d "Mount partition with specified label"
complete -x -c mount -s U -d "Mount partition with specified UID"
complete -c mount -s O -x -d "Exclude filesystems"
complete -c mount -l bind -f -d "Remount a subtree to a second position"
complete -c mount -l move -f -d "Move a subtree to a new position"
complete -c mount -x -s t -d "Filesystem" -a $__fish_filesystems

complete -c mount -x -s o -d "Mount option" -a "(__fish_append ',' $__fish_mount_opts)"

set -g __fish_mount_opts async\tUse\ asynchronous\ I/O atime\tUpdate\ time\ on\ each\ access auto\tMounted\ with\ -a defaults\tUse\ default\ options dev\tInterpret\ character/block\ special\ devices exec\tPermit\ executables _netdev\tFilesystem\ uses\network noatime\tDo\ not\ update\ time\ on\ each\ access noauto\tNot\ mounted\ by\ -a nodev\tDo\ not\ interpret\ character/block\ special\ devices noexec\tDo\ not\ permit\ executables nosuid\tIgnore\ suid\ bits nouser\tOnly\ root\ may\ mount remount\tRemount\ read-only\ filesystem ro\tMount\ read-only rw\tMount\ read-write suid\tAllow\ suid\ bits sync\tUse\ synchronous\ I/O dirsync\tUse\ synchronous\ directory\ operations user\tAny\ user\ may\ mount users\tAny\ user\ may\ mount\ and\ unmount


