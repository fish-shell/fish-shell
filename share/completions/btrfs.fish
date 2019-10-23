# Completions for btrfs-progs
# Author: Akatsuki (akiirui) <imaykiller@gmail.com>

# Todo:
# 1. Detailed options per command
# 2. Simplified descriptions
# ...and more

# Filter the completions per command groups
function __btrfs_options_groups
    if test -z $argv
        return 1
    end
    set -l cmd (commandline -cp)
    if string match -qr -- "$argv\s+\S*\$" $cmd
        return 0
    end
    return 1
end

# Primary command
complete -f -c btrfs -n '__fish_is_first_arg' -a check -d 'Check structural integrity of a filesystem (unmounted).'
complete -f -c btrfs -n '__fish_is_first_arg' -a restore -d 'Try to restore files from a damaged filesystem (unmounted)'
complete -f -c btrfs -n '__fish_is_first_arg' -a send -d 'Send the subvolume(s) to stdout.'
complete -f -c btrfs -n '__fish_is_first_arg' -a receive -d 'Receive subvolumes from a stream'
complete -f -c btrfs -n '__fish_is_first_arg' -a help -d 'Display help information'
complete -f -c btrfs -n '__fish_is_first_arg' -a version -d 'Display btrfs-progs version'

# Primary command groups
complete -f -c btrfs -n '__fish_is_first_arg' -a subvolume -d 'manage subvolumes: create, delete, list, etc'
complete -f -c btrfs -n '__fish_is_first_arg' -a filesystem -d 'overall filesystem tasks and information'
complete -f -c btrfs -n '__fish_is_first_arg' -a balance -d 'balance data across devices, or change block groups using filters'
complete -f -c btrfs -n '__fish_is_first_arg' -a device -d 'manage and query devices in the filesystem'
complete -f -c btrfs -n '__fish_is_first_arg' -a scrub -d 'verify checksums of data and metadata'
complete -f -c btrfs -n '__fish_is_first_arg' -a rescue -d 'toolbox for specific rescue operations'
complete -f -c btrfs -n '__fish_is_first_arg' -a inspect-internal -d 'query various internal information'
complete -f -c btrfs -n '__fish_is_first_arg' -a property -d 'modify properties of filesystem objects'
complete -f -c btrfs -n '__fish_is_first_arg' -a quota -d 'manage filesystem quota settings'
complete -f -c btrfs -n '__fish_is_first_arg' -a qgroup -d 'manage quota groups'
complete -f -c btrfs -n '__fish_is_first_arg' -a replace -d 'replace a device in the filesystem'

set -l subvolume '__btrfs_options_groups subvolume'
set -l filesystem '__btrfs_options_groups filesystem'
set -l balance '__btrfs_options_groups balance'
set -l device '__btrfs_options_groups device'
set -l scrub '__btrfs_options_groups scrub'
set -l rescue '__btrfs_options_groups rescue'
set -l inspect_internal "__btrfs_options_groups inspect-internal"
set -l property '__btrfs_options_groups property'
set -l quota '__btrfs_options_groups quota'
set -l qgroup '__btrfs_options_groups qgroup'
set -l replace '__btrfs_options_groups replace'

# btrfs subvolume
complete -f -c btrfs -n $subvolume -a create -d 'Create a subvolume'
complete -f -c btrfs -n $subvolume -a delete -d 'Delete subvolume(s)'
complete -f -c btrfs -n $subvolume -a list -d 'List subvolumes and snapshots in the filesystem.'
complete -f -c btrfs -n $subvolume -a snapshot -d 'Create a snapshot of the subvolume'
complete -f -c btrfs -n $subvolume -a get-default -d 'Get the default subvolume of a filesystem'
complete -f -c btrfs -n $subvolume -a set-default -d 'Set the default subvolume of the filesystem mounted as default.'
complete -f -c btrfs -n $subvolume -a find-new -d 'List the recently modified files in a filesystem'
complete -f -c btrfs -n $subvolume -a show -d 'Show more information about the subvolume (UUIDs, generations, times, snapshots)'
complete -f -c btrfs -n $subvolume -a sync -d 'Wait until given subvolume(s) are completely removed from the filesystem.'

# btrfs filesystem
complete -f -c btrfs -n $filesystem -a df -d 'Show space usage information for a mount point'
complete -f -c btrfs -n $filesystem -a du -d 'Summarize disk usage of each file.'
complete -f -c btrfs -n $filesystem -a show -d 'Show the structure of a filesystem'
complete -f -c btrfs -n $filesystem -a sync -d 'Force a sync on a filesystem'
complete -f -c btrfs -n $filesystem -a defragment -d 'Defragment a file or a directory'
complete -f -c btrfs -n $filesystem -a resize -d 'Resize a filesystem'
complete -f -c btrfs -n $filesystem -a label -d 'Get or change the label of a filesystem'
complete -f -c btrfs -n $filesystem -a usage -d 'Show detailed information about internal filesystem usage.'

# btrfs balance
complete -f -c btrfs -n $balance -a start -d 'Balance chunks across the devices'
complete -f -c btrfs -n $balance -a pause -d 'Pause running balance'
complete -f -c btrfs -n $balance -a cancel -d 'Cancel running or paused balance'
complete -f -c btrfs -n $balance -a resume -d 'Resume interrupted balance'
complete -f -c btrfs -n $balance -a status -d 'Show status of running or paused balance'

# btrfs device
complete -f -c btrfs -n $device -a add -d 'Add one or more devices to a mounted filesystem.'
complete -f -c btrfs -n $device -a delete -d 'Remove a device from a filesystem'
complete -f -c btrfs -n $device -a remove -d 'Remove a device from a filesystem'
complete -f -c btrfs -n $device -a scan -d 'Scan or forget (unregister) devices of btrfs filesystems'
complete -f -c btrfs -n $device -a ready -d 'Check and wait until a group of devices of a filesystem is ready for mount'
complete -f -c btrfs -n $device -a stats -d 'Show device IO error statistics'
complete -f -c btrfs -n $device -a usage -d 'Show detailed information about internal allocations in devices.'

# btrfs scrub
complete -f -c btrfs -n $scrub -a start -d 'Start a new scrub. If a scrub is already running, the new one fails.'
complete -f -c btrfs -n $scrub -a cancel -d 'Cancel a running scrub'
complete -f -c btrfs -n $scrub -a resume -d 'Resume previously canceled or interrupted scrub'
complete -f -c btrfs -n $scrub -a status -d 'Show status of running or finished scrub'

# btrfs rescue
complete -f -c btrfs -n $rescue -a chunk-recover -d 'Recover the chunk tree by scanning the devices one by one.'
complete -f -c btrfs -n $rescue -a super-recover -d 'Recover bad superblocks from good copies'
complete -f -c btrfs -n $rescue -a zero-log -d 'Clear the tree log. Usable if it\'s corrupted and prevents mount.'
complete -f -c btrfs -n $rescue -a fix-device-size -d 'Re-align device and super block sizes. Usable if newer kernel refuse to mount it due to mismatch super size'

# btrfs inspect-internal
complete -f -c btrfs -n $inspect_internal -a inode-resolve -d 'Get file system paths for the given inode'
complete -f -c btrfs -n $inspect_internal -a logical-resolve -d 'Get file system paths for the given logical address'
complete -f -c btrfs -n $inspect_internal -a subvolid-resolve -d 'Get file system paths for the given subvolume ID.'
complete -f -c btrfs -n $inspect_internal -a rootid -d 'Get tree ID of the containing subvolume of path.'
complete -f -c btrfs -n $inspect_internal -a min-dev-size -d 'Get the minimum size the device can be shrunk to. The device id 1 is used by default.'
complete -f -c btrfs -n $inspect_internal -a dump-tree -d 'Dump tree structures from a given device'
complete -f -c btrfs -n $inspect_internal -a dump-super -d 'Dump superblock from a device in a textual form'
complete -f -c btrfs -n $inspect_internal -a tree-stats -d 'Print various stats for trees'

# btrfs property
complete -f -c btrfs -n $property -a get -d 'Get a property value of a btrfs object'
complete -f -c btrfs -n $property -a set -d 'Set a property on a btrfs object'
complete -f -c btrfs -n $property -a list -d 'Lists available properties with their descriptions for the given object'

# btrfs quota
complete -f -c btrfs -n $quota -a enable -d 'Enable subvolume quota support for a filesystem.'
complete -f -c btrfs -n $quota -a disable -d 'Disable subvolume quota support for a filesystem.'
complete -f -c btrfs -n $quota -a rescan -d 'Trash all qgroup numbers and scan the metadata again with the current config.'

# btrfs qgroup
complete -f -c btrfs -n $qgroup -a assign -d 'Assign SRC as the child qgroup of DST'
complete -f -c btrfs -n $qgroup -a remove -d 'Remove a child qgroup SRC from DST.'
complete -f -c btrfs -n $qgroup -a create -d 'Create a subvolume quota group.'
complete -f -c btrfs -n $qgroup -a destroy -d 'Destroy a quota group.'
complete -f -c btrfs -n $qgroup -a show -d 'Show subvolume quota groups.'
complete -f -c btrfs -n $qgroup -a limit -d 'Set the limits a subvolume quota group.'

# btrfs replace
complete -f -c btrfs -n $replace -a start -d 'Replace device of a btrfs filesystem.'
complete -f -c btrfs -n $replace -a status -d 'Print status and progress information of a running device replace'
complete -f -c btrfs -n $replace -a cancel -d 'Cancel a running device replace operation.'
