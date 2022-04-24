function __fish_print_filesystems -d "Print a list of all known filesystem types"
    set -l fs adfs affs autofs btrfs coda coherent cramfs devpts efs ext ext2 ext3 ffs
    set -a fs hfs hpfs iso9660 jfs minix msdos ncpfs nfs ntfs proc qnx4 ramfs
    set -a fs reiserfs romfs smbfs sysv tmpfs udf ufs umsdos vfat xenix xfs xiafs
    # Mount has helper binaries to mount filesystems
    # These are called mount.* and are placed somewhere in $PATH
    set -l mountfs $PATH/mount.* $PATH/mount_*
    printf '%s\n' $fs (string replace -ra '.*/mount[._]' '' -- $mountfs)
end
