
function __fish_print_filesystems -d "Print a list of all known filesystem types"
	set -l fs adfs affs autofs coda coherent cramfs devpts efs ext ext2 ext3
	set fs $fs hfs hpfs iso9660 jfs minix msdos ncpfs nfs ntfs proc qnx4 ramfs
	set fs $fs reiserfs romfs smbfs sysv tmpfs udf ufs umsdos vfat xenix xfs xiafs
	# Mount has helper binaries to mount filesystems
	# These are called mount.* and are placed somewhere in $PATH
	printf "%s\n" $fs (string replace -ra ".*/mount." "" -- $PATH/mount.*)
end
