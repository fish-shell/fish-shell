# completion for diskutil (macOS)

#############
# Functions #
#############

function __fish_diskutil_devices
    set -l mountpoints /dev/disk*
    printf '%s\n' $mountpoints
end

function __fish_diskutil_mounted_volumes
    set -l mountpoints /Volumes/*
    printf '%s\n' $mountpoints
end

function __fish_diskutil_using_not_subcommand
    not __fish_seen_subcommand_from apfs
    and not __fish_seen_subcommand_from appleRAID
    and not __fish_seen_subcommand_from coreStorage
    and __fish_seen_subcommand_from $argv
end

############
# Commands #
############

# list
complete -f -c diskutil -n __fish_use_subcommand -a list -d 'List disks'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand list' -o plist -d 'Return a property list'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand list' -a '(__fish_diskutil_devices)'

# info
complete -f -c diskutil -n __fish_use_subcommand -a info -d 'Get detailed information about a specific whole disk or partition'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand info' -o plist -d 'Return a property list'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand info' -a '(__fish_diskutil_devices)'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand info' -o all -d 'Process all disks'

# activity
complete -f -c diskutil -n __fish_use_subcommand -a activity -d 'Continuously display system-wide disk manipulation activity'

# listFilesystems
complete -f -c diskutil -n __fish_use_subcommand -a listFilesystems -d 'Show the file system personalities available'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand listFilesystems' -o plist -d 'Return a property list'

# umount
complete -f -c diskutil -n __fish_use_subcommand -a umount -d 'Unmount a single volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand umount' -a '(__fish_diskutil_mounted_volumes)'

# umountDisk
complete -f -c diskutil -n __fish_use_subcommand -a umountDisk -d 'Unmount an entire disk (all volumes)'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand umountDisk' -a '(__fish_diskutil_mounted_volumes)'

# eject
complete -f -c diskutil -n __fish_use_subcommand -a eject -d 'Eject a volume or disk'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand eject' -a '(__fish_diskutil_mounted_volumes ; __fish_diskutil_devices)'

# mount
complete -f -c diskutil -n __fish_use_subcommand -a mount -d 'Mount a single volume'
complete -r -c diskutil -n '__fish_diskutil_using_not_subcommand mount' -o mountPoint -d 'Specify mount point'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand mount' -a '(__fish_diskutil_devices)'

# mountDisk
complete -f -c diskutil -n __fish_use_subcommand -a mountDisk -d 'Mount an entire disk (all mountable volumes)'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand mountDisk' -a '(__fish_diskutil_devices)'

# rename
complete -f -c diskutil -n __fish_use_subcommand -a rename -d 'Rename a volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand rename' -a '(__fish_diskutil_devices)'

# enableJournal
complete -f -c diskutil -n __fish_use_subcommand -a enableJournal -d 'Enable journaling on an HFS+ volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand enableJournal' -a '(__fish_diskutil_devices)'

# disableJournal
complete -f -c diskutil -n __fish_use_subcommand -a disableJournal -d 'Disable journaling on an HFS+ volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand disableJournal' -a '(__fish_diskutil_devices)'

# moveJournal
complete -f -c diskutil -n __fish_use_subcommand -a moveJournal -d 'Create a 512MB Apple_Journal partition'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand moveJournal' -a '(__fish_diskutil_devices)'

# enableOwnership
complete -f -c diskutil -n __fish_use_subcommand -a enableOwnership -d 'Enable ownership of a volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand enableOwnership' -a '(__fish_diskutil_devices)'

# disableOwnership
complete -f -c diskutil -n __fish_use_subcommand -a disableOwnership -d 'Disable ownership of a volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand disableOwnership' -a '(__fish_diskutil_devices)'

# verifyVolume
complete -f -c diskutil -n __fish_use_subcommand -a verifyVolume -d 'Verify the file system data structures of a volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand verifyVolume' -a '(__fish_diskutil_devices)'

# repairVolume
complete -f -c diskutil -n __fish_use_subcommand -a repairVolume -d 'Repair the file system data structures of a volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand repairVolume' -a '(__fish_diskutil_devices)'

# verifyDisk
complete -f -c diskutil -n __fish_use_subcommand -a verifyDisk -d 'Verify the partition map layout of a whole disk'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand verifyDisk' -a '(__fish_diskutil_devices)'

# repairDisk
complete -f -c diskutil -n __fish_use_subcommand -a repairDisk -d 'Repair the partition map layout of a whole disk'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand repairDisk' -a '(__fish_diskutil_devices)'

# eraseDisk
complete -f -c diskutil -n __fish_use_subcommand -a eraseDisk -d 'Erase an existing disk'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand eraseDisk' -a '(__fish_diskutil_devices)'

# eraseVolume
complete -f -c diskutil -n __fish_use_subcommand -a verifyDisk -d 'Write out a new empty file system volume'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand verifyDisk' -a '(__fish_diskutil_devices)'

# reformat
complete -f -c diskutil -n __fish_use_subcommand -a reformat -d 'Erase an existing volume by writing out a new empty file system'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand reformat' -a '(__fish_diskutil_devices)'

# eraseOptical
complete -f -c diskutil -n __fish_use_subcommand -a eraseOptical -d 'Erase optical media (CD/RW, DVD/RW, etc.)'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand eraseOptical' -a '(__fish_diskutil_devices)'

# zeroDisk
complete -f -c diskutil -n __fish_use_subcommand -a zeroDisk -d 'Erase a device, writing zeros to the media'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand zeroDisk' -a '(__fish_diskutil_devices)'

# randomDisk
complete -f -c diskutil -n __fish_use_subcommand -a randomDisk -d 'Erase a whole disk, writing random data to the media'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand randomDisk' -a '(__fish_diskutil_devices)'

# secureErase
complete -f -c diskutil -n __fish_use_subcommand -a secureErase -d 'Erase, using a secure method'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand secureErase' -a '(__fish_diskutil_devices)'

# partitionDisk
complete -f -c diskutil -n __fish_use_subcommand -a partitionDisk -d '(re)Partition a disk, removing all volumes'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand partitionDisk' -a '(__fish_diskutil_devices)'

# resizeVolume
complete -f -c diskutil -n __fish_use_subcommand -a resizeVolume -d 'Non-destructively resize a volume (partition)'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand resizeVolume' -a '(__fish_diskutil_devices)'

# splitPartition
complete -f -c diskutil -n __fish_use_subcommand -a splitPartition -d 'Destructively split a volume into multiple partitions'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand splitPartition' -a '(__fish_diskutil_devices)'

# mergePartitions
complete -f -c diskutil -n __fish_use_subcommand -a mergePartitions -d 'Merge two or more partitions on a disk'
complete -f -c diskutil -n '__fish_diskutil_using_not_subcommand mergePartitions' -a '(__fish_diskutil_devices)'

# apfs
complete -f -c diskutil -n __fish_use_subcommand -a apfs -d 'Merge two or more partitions on a disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a list -d 'Show status of all current APFS Containers'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a convert -d 'Nondestructively convert from HFS to APFS'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a create -d 'Create a new APFS Container with one APFS Volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a createContainer -d 'Create a new empty APFS Container'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a deleteContainer -d 'Delete an APFS Container and reformat disks to HFS'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a resizeContainer -d 'Resize an APFS Container and its disk space usage'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a addVolume -d 'Export a new APFS Volume from an APFS Container'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a deleteVolume -d 'Remove an APFS Volume from its APFS Container'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a eraseVolume -d 'Erase contents of, but keep, an APFS Volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a changeVolumeRole -d 'Change the Role metadata bits of an APFS Volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a unlockVolume -d 'Unlock an encrypted APFS Volume which is locked'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a lockVolume -d 'Lock an encrypted APFS Volume (diskutil unmount)'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a listCryptoUsers -d 'List cryptographic users of encrypted APFS Volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a changePassphrase -d 'Change the passphrase of a cryptographic user'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a setPassphraseHint -d 'Set or clear passphrase hint of a cryptographic user'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a encryptVolume -d 'Start async encryption of an unencrypted APFS Volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a decryptVolume -d 'Start async decryption of an encrypted APFS Volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from apfs' -a updatePreboot -d 'Update the APFS Volume\'s related APFS Preboot Volume'

# appleRAID
complete -f -c diskutil -n __fish_use_subcommand -a appleRAID -d 'Create, manipulate and destroy AppleRAID volumes'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a list -d 'Display the current status of RAID sets'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a create -d 'Create a RAID set on multiple disks'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a delete -d 'Delete an existing RAID set'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a repairMirror -d 'Repair a damaged RAID mirror set'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a add -d 'Add a spare or member disk to an existing RAID'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a remove -d 'Remove a spare or member disk from an existing RAID'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a enable -d 'Convert a volume into a single disk RAID set'
complete -f -c diskutil -n '__fish_seen_subcommand_from appleRAID' -a update -d 'Update the settings of an existing RAID'

# coreStorage
complete -f -c diskutil -n __fish_use_subcommand -a coreStorage -d 'Create, manipulate and destroy CoreStorage volumes'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a list -d 'Show status of CoreStorage volumes'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a info -d 'Get CoreStorage information by UUID or disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a convert -d 'Convert a volume into a CoreStorage volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a revert -d 'Revert a CoreStorage volume to its native type'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a create -d 'Create a new CoreStorage logical volume group'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a delete -d 'Delete a CoreStorage logical volume group'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a rename -d 'Rename a CoreStorage logical volume group'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a createVolume -d 'Create a new CoreStorage logical volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a deleteVolume -d 'Delete a volume from a logical volume group'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a encryptVolume -d 'Start encrypting a CoreStorage logical volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a decryptVolume -d 'Start decrypting a CoreStorage logical volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a unlockVolume -d 'Attach/mount a locked CoreStorage logical volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a changeVolumePassphrase -d 'Change a CoreStorage logical volume\'s passphrase'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a resizeVolume -d 'Resize a CoreStorage volume'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a resizeDisk -d 'Resize a CoreStorage physical volume disk'
complete -f -c diskutil -n '__fish_seen_subcommand_from coreStorage' -a resizeStack -d 'Resize a CoreStorage logical/physical volume set'
