#completion for diskutil

function __fish_diskutil_devices
    set -l mountpoints /dev/disk*; printf '%s\n' $mountpoints
end

function __fish_diskutil_mounted_volumes
    set -l mountpoints /Volumes/*; printf '%s\n' $mountpoints
end

# list
complete -f -c diskutil -n '__fish_use_subcommand' -a list -d 'List disks'
complete -f -c diskutil -n '__fish_seen_subcommand_from list' -o 'plist' -d 'Return a property list'
complete -f -c diskutil -n '__fish_seen_subcommand_from list' -a '(__fish_diskutil_devices)'

# info
complete -f -c diskutil -n '__fish_use_subcommand' -a info -d 'Get detailed information about a specific whole disk or partition'
complete -f -c diskutil -n '__fish_seen_subcommand_from info' -o 'plist' -d 'Return a property list'
complete -f -c diskutil -n '__fish_seen_subcommand_from info' -a '(__fish_diskutil_devices)'
complete -f -c diskutil -n '__fish_seen_subcommand_from info' -o 'all' -d 'Process all disks'

# activity
complete -f -c diskutil -n '__fish_use_subcommand' -a activity -d 'Continuously display system-wide disk manipulation activity'

# umount
complete -f -c diskutil -n '__fish_use_subcommand' -a umount -d 'Unmount a single volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from umount' -a '(__fish_diskutil_mounted_volumes)'

# umountDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a umountDisk -d 'Unmount an entire disk (all volumes)'
complete -f -c diskutil -n '__fish_seen_subcommand_from umountDisk' -a '(__fish_diskutil_mounted_volumes)'

# eject
complete -f -c diskutil -n '__fish_use_subcommand' -a eject -d 'Eject a disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from eject' -a '(__fish_diskutil_devices)'

# mount
complete -f -c diskutil -n '__fish_use_subcommand' -a mount -d 'Mount a single volume'
complete -r -c diskutil -n '__fish_seen_subcommand_from mount' -o 'mountPoint' -d 'Specify mount point'
complete -f -c diskutil -n '__fish_seen_subcommand_from mount' -a '(__fish_diskutil_devices)'

# mountDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a mountDisk -d 'Mount an entire disk (all mountable volumes)'
complete -f -c diskutil -n '__fish_seen_subcommand_from mountDisk' -a '(__fish_diskutil_devices)'

# rename
complete -f -c diskutil -n '__fish_use_subcommand' -a rename -d 'Rename a volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from rename' -a '(__fish_diskutil_devices)'

# enableJournal
complete -f -c diskutil -n '__fish_use_subcommand' -a enableJournal -d 'Enable journaling on an HFS+ volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from enableJournal' -a '(__fish_diskutil_devices)'

# disableJournal
complete -f -c diskutil -n '__fish_use_subcommand' -a disableJournal -d 'Disable journaling on an HFS+ volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from disableJournal' -a '(__fish_diskutil_devices)'

# moveJournal
complete -f -c diskutil -n '__fish_use_subcommand' -a moveJournal -d 'Create a 512MB Apple_Journal partition'
complete -f -c diskutil -n '__fish_seen_subcommand_from moveJournal' -a '(__fish_diskutil_devices)'

# enableOwnership
complete -f -c diskutil -n '__fish_use_subcommand' -a enableOwnership -d 'Enable ownership of a volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from enableOwnership' -a '(__fish_diskutil_devices)'

# disableOwnership
complete -f -c diskutil -n '__fish_use_subcommand' -a disableOwnership -d 'Disable ownership of a volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from disableOwnership' -a '(__fish_diskutil_devices)'

# verifyVolume
complete -f -c diskutil -n '__fish_use_subcommand' -a verifyVolume -d 'Verify the file system data structures of a volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from verifyVolume' -a '(__fish_diskutil_devices)'

# repairVolume
complete -f -c diskutil -n '__fish_use_subcommand' -a repairVolume -d 'Repair the file system data structures of a volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from repairVolume' -a '(__fish_diskutil_devices)'

# verifyDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a verifyDisk -d 'Verify the partition map layout of a whole disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from verifyDisk' -a '(__fish_diskutil_devices)'

# repairDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a repairDisk -d 'Repair the partition map layout of a whole disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from repairDisk' -a '(__fish_diskutil_devices)'

# eraseDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a eraseDisk -d 'Erase an existing disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from eraseDisk' -a '(__fish_diskutil_devices)'

# eraseVolume
complete -f -c diskutil -n '__fish_use_subcommand' -a verifyDisk -d 'Write out a new empty file system volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from verifyDisk' -a '(__fish_diskutil_devices)'

# reformat
complete -f -c diskutil -n '__fish_use_subcommand' -a reformat -d 'Erase an existing volume by writing out a new empty file system'
complete -f -c diskutil -n '__fish_seen_subcommand_from reformat' -a '(__fish_diskutil_devices)'

# eraseOptical
complete -f -c diskutil -n '__fish_use_subcommand' -a eraseOptical -d 'Erase optical media (CD/RW, DVD/RW, etc.)'
complete -f -c diskutil -n '__fish_seen_subcommand_from eraseOptical' -a '(__fish_diskutil_devices)'

# zeroDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a zeroDisk -d 'Erase a device, writing zeros to the media'
complete -f -c diskutil -n '__fish_seen_subcommand_from zeroDisk' -a '(__fish_diskutil_devices)'

# randomDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a randomDisk -d 'Erase a whole disk, writing random data to the media'
complete -f -c diskutil -n '__fish_seen_subcommand_from randomDisk' -a '(__fish_diskutil_devices)'

# secureErase
complete -f -c diskutil -n '__fish_use_subcommand' -a secureErase -d 'Erase, using a secure method'
complete -f -c diskutil -n '__fish_seen_subcommand_from secureErase' -a '(__fish_diskutil_devices)'

# partitionDisk
complete -f -c diskutil -n '__fish_use_subcommand' -a partitionDisk -d '(re)Partition a disk, removing all volumes'
complete -f -c diskutil -n '__fish_seen_subcommand_from partitionDisk' -a '(__fish_diskutil_devices)'

# resizeVolume
complete -f -c diskutil -n '__fish_use_subcommand' -a resizeVolume -d 'Non-destructively resize a volume (partition)'
complete -f -c diskutil -n '__fish_seen_subcommand_from resizeVolume' -a '(__fish_diskutil_devices)'

# splitPartition
complete -f -c diskutil -n '__fish_use_subcommand' -a splitPartition -d 'Destructively split a volume into multiple partitions'
complete -f -c diskutil -n '__fish_seen_subcommand_from splitPartition' -a '(__fish_diskutil_devices)'

# mergePartitions
complete -f -c diskutil -n '__fish_use_subcommand' -a mergePartitions -d 'Merge two or more partitions on a disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from mergePartitions' -a '(__fish_diskutil_devices)'

# appleRAID
complete -f -c diskutil -n '__fish_use_subcommand' -a appleRAID -d 'Create, manipulate and destroy AppleRAID volumes'

# coreStorage
complete -f -c diskutil -n '__fish_use_subcommand' -a coreStorage -d 'Create, manipulate and destroy CoreStorage volumes'
