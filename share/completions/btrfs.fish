# Completions for btrfs-progs

# Todo:
# - (Done) Detailed options per command
# - Simplified duplicate options completion with `for`
# - Simplified descriptions

# Filter the completions per primary command
function __btrfs_commands
    if not set -q argv[1]
        return 1
    end
    set -l cmd (commandline -cp)
    if string match -qr -- "btrfs\s+$argv[1]\s+\S*" $cmd
        return 0
    end
    return 1
end

# Filter the completions per command group
function __btrfs_command_groups
    if not set -q argv[1]
        return 1
    end
    set -l cmd (commandline -cp)
    if set -q argv[2]
        if string match -qr -- "btrfs\s+$argv[1]\s+$argv[2]\s+\S*" $cmd
            return 0
        end
        return 1
    end
    if string match -qr -- "btrfs\s+$argv[1]\s+\S*\$" $cmd
        return 0
    end
    return 1
end

set -l check '__btrfs_commands check'
set -l restore '__btrfs_commands restore'
set -l send '__btrfs_commands send'
set -l receive '__btrfs_commands receive'
set -l help '__btrfs_commands help'

set -l subvolume '__btrfs_command_groups subvolume'
set -l filesystem '__btrfs_command_groups filesystem'
set -l balance '__btrfs_command_groups balance'
set -l device '__btrfs_command_groups device'
set -l scrub '__btrfs_command_groups scrub'
set -l rescue '__btrfs_command_groups rescue'
set -l inspect_internal '__btrfs_command_groups inspect-internal'
set -l property '__btrfs_command_groups property'
set -l quota '__btrfs_command_groups quota'
set -l qgroup '__btrfs_command_groups qgroup'
set -l replace '__btrfs_command_groups replace'

# Global command
complete -f -c btrfs -l help -d 'Display help information'

# Primary command
complete -f -c btrfs -n __fish_is_first_arg -a check -d 'Check structural integrity of a filesystem (unmounted).'
complete -f -c btrfs -n __fish_is_first_arg -a restore -d 'Try to restore files from a damaged filesystem (unmounted)'
complete -f -c btrfs -n __fish_is_first_arg -a send -d 'Send the subvolume(s) to stdout.'
complete -f -c btrfs -n __fish_is_first_arg -a receive -d 'Receive subvolumes from a stream'
complete -f -c btrfs -n __fish_is_first_arg -a help -d 'Display help information'
complete -f -c btrfs -n __fish_is_first_arg -a version -d 'Display btrfs-progs version'

# Primary command groups
complete -f -c btrfs -n __fish_is_first_arg -a subvolume -d 'manage subvolumes: create, delete, list, etc'
complete -f -c btrfs -n __fish_is_first_arg -a filesystem -d 'overall filesystem tasks and information'
complete -f -c btrfs -n __fish_is_first_arg -a balance -d 'balance data across devices, or change block groups using filters'
complete -f -c btrfs -n __fish_is_first_arg -a device -d 'manage and query devices in the filesystem'
complete -f -c btrfs -n __fish_is_first_arg -a scrub -d 'verify checksums of data and metadata'
complete -f -c btrfs -n __fish_is_first_arg -a rescue -d 'toolbox for specific rescue operations'
complete -f -c btrfs -n __fish_is_first_arg -a inspect-internal -d 'query various internal information'
complete -f -c btrfs -n __fish_is_first_arg -a property -d 'modify properties of filesystem objects'
complete -f -c btrfs -n __fish_is_first_arg -a quota -d 'manage filesystem quota settings'
complete -f -c btrfs -n __fish_is_first_arg -a qgroup -d 'manage quota groups'
complete -f -c btrfs -n __fish_is_first_arg -a replace -d 'replace a device in the filesystem'

# btrfs check
complete -f -c btrfs -n $check -s s -l super -d 'Use this SUPERBLOCK copy'
complete -f -c btrfs -n $check -s b -l backup -d 'Use the first valid BACKUP root copy'
complete -f -c btrfs -n $check -s r -l tree-root -d 'Use the given bytenr for the TREE root'
complete -f -c btrfs -n $check -l chunk-root -d 'Use the given bytenr for the CHUNK-TREE root'
complete -f -c btrfs -n $check -l readonly -d 'Run in read-only mode'
complete -f -c btrfs -n $check -l repair -d 'Try to repair the filesystem'
complete -f -c btrfs -n $check -l force -d 'Skip mount checks, repair is not possible'
complete -f -c btrfs -n $check -l mode -d 'Allows choice of memory/IO trade-offs' -ra '{original,lowmem}'
complete -f -c btrfs -n $check -l init-csum-tree -d 'Create a new CRC tree (repair only)'
complete -f -c btrfs -n $check -l init-extent-tree -d 'Create a new extent tree (repair only)'
complete -f -c btrfs -n $check -l clear-space-cache -d 'clear space cache (repair only)' -ra '{v1,v2}'
complete -f -c btrfs -n $check -l check-data-csum -d 'Verify checksums of data blocks'
complete -f -c btrfs -n $check -s Q -l qgroup-report -d 'Print a report on qgroup consistency'
complete -f -c btrfs -n $check -s E -l subvol-extents -d 'Print subvolume extents and sharing state'
complete -f -c btrfs -n $check -s p -l progress -d 'Indicate progress'

# btrfs restore
complete -f -c btrfs -n $restore -s s -l snapshots -d 'Get snapshots'
complete -f -c btrfs -n $restore -s x -l xattr -d 'Restore extended attributes'
complete -f -c btrfs -n $restore -s m -l metadata -d 'Restore owner, mode and times'
complete -f -c btrfs -n $restore -s S -l symlink -d 'Restore symbolic links'
complete -f -c btrfs -n $restore -s v -l verbose -d Verbose
complete -f -c btrfs -n $restore -s i -l ignore-errors -d 'Ignore errors'
complete -f -c btrfs -n $restore -s o -l overwrite -d Overwrite
complete -f -c btrfs -n $restore -s t -r -d 'Tree location'
complete -f -c btrfs -n $restore -s f -r -d 'Filesystem location'
complete -f -c btrfs -n $restore -s u -l super -r -d 'Super mirror'
complete -f -c btrfs -n $restore -s r -l root -r -d 'Root objectid'
complete -f -c btrfs -n $restore -s d -d 'Find dir'
complete -f -c btrfs -n $restore -s l -l list-roots -d 'List tree roots'
complete -f -c btrfs -n $restore -s D -l dry-run -d 'Only list files that would be recovered'
complete -f -c btrfs -n $restore -l path-regex -r -d 'Restore only filenames matching regex'
complete -f -c btrfs -n $restore -s c -d 'Ignore case (--path-regex only)'

# btrfs send
complete -f -c btrfs -n $send -s e -d ''
complete -f -c btrfs -n $send -s p -r -d 'Send an incremental stream from <parent> to <subvol>'
complete -f -c btrfs -n $send -s c -r -d 'Use this snapshot as a clone source for an incremental send'
complete -f -c btrfs -n $send -s f -r -d 'Output is normally written to stdout'
complete -f -c btrfs -n $send -l no-data -d 'send in NO_FILE_DATA mode'
complete -f -c btrfs -n $send -s v -l verbose -d 'Enable verbose output to stderr'
complete -f -c btrfs -n $send -s q -l quiet -d 'Suppress all messages, except errors'
complete -f -c btrfs -n $send -l proto -a '0 1 2' -r -d 'Use send protocol version'
complete -f -c btrfs -n $send -l proto -l compressed-data -d 'Send compressed data directly'

# btrfs receive
complete -f -c btrfs -n $receive -s v -d 'Increase verbosity about performed actions'
complete -f -c btrfs -n $receive -s q -l quiet -d 'Suppress all messages, except errors'
complete -f -c btrfs -n $receive -s f -r -d 'Read the stream from FILE instead of stdin'
complete -f -c btrfs -n $receive -s e -d 'Terminate after receiving an <end cmd> marker in the stream'
complete -f -c btrfs -n $receive -s C -l chroot -d 'Confine the process to <mount> using chroot'
complete -f -c btrfs -n $receive -s E -l max-errors -r -d 'Terminate when NUMBER errors occur'
complete -f -c btrfs -n $receive -s m -r -d 'The root mount point of the destination filesystem'
complete -f -c btrfs -n $receive -l force-decompress -r -d 'Always decompress data'
complete -f -c btrfs -n $receive -l dump -d 'Dump stream metadata'

# btrfs help
complete -f -c btrfs -n $help -l full -d 'Display detailed help on every command'
complete -f -c btrfs -n $help -l box -d 'Show list of built-in tools (busybox style)'

# btrfs subvolume
complete -f -c btrfs -n $subvolume -a create -d 'Create a subvolume'
complete -f -c btrfs -n $subvolume -a delete -d 'Delete subvolume(s)'
complete -f -c btrfs -n $subvolume -a list -d 'List subvolumes and snapshots in the filesystem.'
complete -f -c btrfs -n $subvolume -a snapshot -d 'Create a snapshot of the subvolume'
complete -f -c btrfs -n $subvolume -a get-default -d 'Get the default subvolume of a filesystem'
complete -f -c btrfs -n $subvolume -a set-default -d 'Set the default subvolume of the filesystem mounted as default.'
complete -f -c btrfs -n $subvolume -a find-new -d 'List the recently modified files in a filesystem'
complete -f -c btrfs -n $subvolume -a show -d 'Show more information about the subvolume'
complete -f -c btrfs -n $subvolume -a sync -d 'Wait until given subvolume(s) are completely removed from the filesystem.'
# btrfs subvolume create
complete -f -c btrfs -n '__btrfs_command_groups subvolume create' -s i -d 'Add subvolume to a qgroup (can be given multiple times)'
complete -f -c btrfs -n '__btrfs_command_groups subvolume create' -s p -d 'Create any missing parent directories'
# btrfs subvolume delete
complete -f -c btrfs -n '__btrfs_command_groups subvolume delete' -s c -l commit-after -d 'Wait for transaction commit at the end of the operation'
complete -f -c btrfs -n '__btrfs_command_groups subvolume delete' -s C -l commit-each -d 'Wait for transaction commit after deleting each subvolume'
complete -f -c btrfs -n '__btrfs_command_groups subvolume delete' -s R -l recursive -d 'Delete subvolumes beneath each subvolume recursively'
complete -f -c btrfs -n '__btrfs_command_groups subvolume delete' -s v -l verbose -d 'Verbose output of operations'
# btrfs subvolume list
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s o -d 'Print only subvolumes below specified path'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s a -d 'Print all the subvolumes and absolute and relative path'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s p -d 'Print parent ID'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s c -d 'Print the ogeneration of the subvolume'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s g -d 'Print the generation of the subvolume'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s u -d 'Print the uuid of subvolumes (and snapshots)'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s q -d 'Print the parent uuid of the snapshots'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s R -d 'Print the uuid of the received snapshots'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s s -d 'List only snapshots'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s r -d 'List readonly subvolumes (including snapshots)'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s d -d 'List deleted subvolumes that are not yet cleaned'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s t -d 'Print the result as a table'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s G -r -d 'Filter the subvolumes by generation'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -s C -r -d 'Filter the subvolumes by ogeneration'
complete -f -c btrfs -n '__btrfs_command_groups subvolume list' -l sort -d 'List the subvolume in order' -a '{gen,ogen,rootid,path}'
# btrfs subvolume snapshot
complete -f -c btrfs -n '__btrfs_command_groups subvolume snapshot' -s r -d 'Create a readonly snapshot'
complete -f -c btrfs -n '__btrfs_command_groups subvolume snapshot' -s i -d 'Add snapshot to a qgroup (can be given multiple times)'
# btrfs subvolume show
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s r -l rootid -d 'Show rootid of the subvolume'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s u -l uuid -d 'Show uuid of the subvolume'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s b -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s h -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s H -d 'Show human friendly numbers, base 1000'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s k -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s m -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s g -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups subvolume show' -s t -l tbytes -d 'Show sizes in TiB, or TB with --si'
# btrfs subvolume sync
complete -f -c btrfs -n '__btrfs_command_groups subvolume sync' -s s -d 'Sleep NUMBER seconds between checks'

# btrfs filesystem
complete -f -c btrfs -n $filesystem -a df -d 'Show space usage information for a mount point'
complete -f -c btrfs -n $filesystem -a du -d 'Summarize disk usage of each file.'
complete -f -c btrfs -n $filesystem -a show -d 'Show the structure of a filesystem'
complete -f -c btrfs -n $filesystem -a sync -d 'Force a sync on a filesystem'
complete -f -c btrfs -n $filesystem -a defragment -d 'Defragment a file or a directory'
complete -f -c btrfs -n $filesystem -a resize -d 'Resize a filesystem'
complete -f -c btrfs -n $filesystem -a label -d 'Get or change the label of a filesystem'
complete -f -c btrfs -n $filesystem -a usage -d 'Show detailed information about internal filesystem usage.'
complete -f -c btrfs -n $filesystem -a mkswapfile -d 'Create a new swapfile'
# btrfs filesystem df
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -s b -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -s h -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -s H -d 'Show human friendly numbers, base 1000'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -s k -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -s m -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -s g -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem df' -s t -l tbytes -d 'Show sizes in TiB, or TB with --si'
# btrfs filesystem du
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -s s -l summarize -d 'Display only a total for each argument'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem du' -l tbytes -d 'Show sizes in TiB, or TB with --si'
# btrfs filesystem show
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -s d -l all-devices -d 'Show only disks under /dev containing btrfs filesystem'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -s m -l mounted -d 'Show only mounted btrfs'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem show' -l tbytes -d 'Show sizes in TiB, or TB with --si'
# btrfs filesystem defragment
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s v -d 'Be verbose'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s r -d 'Defragment files recursively'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s c -d 'Compress the file while defragmenting' -ra '{zlib,lzo,zstd}'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s f -d 'Flush data to disk immediately after defragmenting'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s s -r -d 'Defragment only from NUMBER byte onward'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s l -r -d 'Defragment only up to LEN bytes'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s t -r -d 'Target extent SIZE hint'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -l step -r -d 'Defragment in steps of SIZE'
complete -f -c btrfs -n '__btrfs_command_groups filesystem defragment' -s L -l level -r -d 'Specify compression levels'

# btrfs filesystem usage
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s b -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s h -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s H -d 'Show human friendly numbers, base 1000'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s k -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s m -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s g -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s t -l tbytes -d 'Show sizes in TiB, or TB with --si'
complete -f -c btrfs -n '__btrfs_command_groups filesystem usage' -s T -d 'Show data in tabular format'
# btrfs filesystem mkswapfile
complete -f -c btrfs -n '__btrfs_command_groups filesystem mkswapfile' -s s -l size -r -d 'Swapfile size'
complete -f -c btrfs -n '__btrfs_command_groups filesystem mkswapfile' -s U -l uuid -r -d 'UUID for the swapfile'
# btrfs filesystem resize
complete -f -c btrfs -n '__btrfs_command_groups filesystem resize' -l enqueue -d 'Wait for other exclusive operations'

# btrfs balance
complete -f -c btrfs -n $balance -a start -d 'Balance chunks across the devices'
complete -f -c btrfs -n $balance -a pause -d 'Pause running balance'
complete -f -c btrfs -n $balance -a cancel -d 'Cancel running or paused balance'
complete -f -c btrfs -n $balance -a resume -d 'Resume interrupted balance'
complete -f -c btrfs -n $balance -a status -d 'Show status of running or paused balance'
# btrfs balance start
function __btrfs_balance_filters
    set -l profiles raid{0,1{,c3,c4},10,5,6} dup single
    set -l btrfs_balance_filters \
        profiles=$profiles\t"Balances only block groups with the given profiles" \
        convert=$profiles\t"Convert selected block groups to given profile" \
        usage= devid= vrange= limit= strips= soft
    set -l prefix (commandline -tc | string replace -r '^-d' -- '' | string match -rg '^(.*?)?[^,]*$' -- $token)
    printf "%s\n" "$prefix"$btrfs_balance_filters
end
complete -f -c btrfs -n '__btrfs_command_groups balance start' -s d -ra '(__btrfs_balance_filters)' -d 'Act on data chunks with FILTERS'
complete -f -c btrfs -n '__btrfs_command_groups balance start' -s m -ra '(__btrfs_balance_filters)' -d 'Act on metadata chunks with FILTERS'
complete -f -c btrfs -n '__btrfs_command_groups balance start' -s s -ra '(__btrfs_balance_filters)' -d 'Act on system chunks with FILTERS (only under -f)'
complete -f -c btrfs -n '__btrfs_command_groups balance start' -s v -d 'Be verbose'
complete -f -c btrfs -n '__btrfs_command_groups balance start' -s f -d 'Force a reduction of metadata integrity'
complete -f -c btrfs -n '__btrfs_command_groups balance start' -l full-balance -d 'Do not print warning and do not delay start'
complete -f -c btrfs -n '__btrfs_command_groups balance start' -l background -l bg -d 'Run the balance as a background process'
complete -f -c btrfs -n '__btrfs_command_groups balance start' -l enqueue -d 'Wait for other exclusive operations'
# btrfs balance status
complete -f -c btrfs -n '__btrfs_command_groups balance status' -s v -d 'Be verbose'

# btrfs device
complete -f -c btrfs -n $device -a add -d 'Add one or more devices to a mounted filesystem.'
complete -f -c btrfs -n $device -a delete -d 'Remove a device from a filesystem'
complete -f -c btrfs -n $device -a remove -d 'Remove a device from a filesystem'
complete -f -c btrfs -n $device -a scan -d 'Scan or forget (unregister) devices of btrfs filesystems'
complete -f -c btrfs -n $device -a ready -d 'Check and wait until a group of devices of a filesystem is ready for mount'
complete -f -c btrfs -n $device -a stats -d 'Show device IO error statistics'
complete -f -c btrfs -n $device -a usage -d 'Show detailed information about internal allocations in devices.'
# btrfs device add
complete -f -c btrfs -n '__btrfs_command_groups device add' -s K -l nodiscard -d 'Do not perform TRIM on DEVICES'
complete -f -c btrfs -n '__btrfs_command_groups device add' -s f -l force -d 'Force overwrite existing filesystem on the disk'
# btrfs device scan
complete -f -c btrfs -n '__btrfs_command_groups device scan' -s d -l all-devices -d 'Enumerate and register all devices'
complete -f -c btrfs -n '__btrfs_command_groups device scan' -s u -l forget -d 'Unregister a given device or all stale devices'
# btrfs device stats
complete -f -c btrfs -n '__btrfs_command_groups device stats' -s c -l check -d 'Return non-zero if any stat counter is not zero'
complete -f -c btrfs -n '__btrfs_command_groups device stats' -s z -l reset -d 'Show current stats and reset values to zero'
complete -f -c btrfs -n '__btrfs_command_groups device stats' -s T -d "Print stats in a tabular form"
# btrfs device remove
complete -f -c btrfs -n '__btrfs_command_groups device remove' -l enqueue -d 'Wait for other exclusive operations'
complete -f -c btrfs -n '__btrfs_command_groups device remove' -l force -d 'Skip the safety timeout for removing multiple devices'
# btrfs device usage
complete -f -c btrfs -n '__btrfs_command_groups device usage' -s b -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -s h -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -s H -d 'Show human friendly numbers, base 1000'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -s k -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -s m -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -s g -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups device usage' -s t -l tbytes -d 'Show sizes in TiB, or TB with --si'

# btrfs scrub
complete -f -c btrfs -n $scrub -a start -d 'Start a new scrub. If a scrub is already running, the new one fails.'
complete -f -c btrfs -n $scrub -a cancel -d 'Cancel a running scrub'
complete -f -c btrfs -n $scrub -a resume -d 'Resume previously canceled or interrupted scrub'
complete -f -c btrfs -n $scrub -a status -d 'Show status of running or finished scrub'
complete -f -c btrfs -n $scrub -a limit -d 'Show or set scrub limits on devices of the given filesystem'
# btrfs scrub limit
complete -f -c btrfs -n '__btrfs_command_groups scrub limit' -s d -l devid -d 'Select the device by DEVID to apply the limit'
complete -f -c btrfs -n '__btrfs_command_groups scrub limit' -s l -l limit -d 'Set the limit of the device'
complete -f -c btrfs -n '__btrfs_command_groups scrub limit' -s a -l all -d 'Apply the limit to all devices'
# btrfs scrub start
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s B -d 'Do not background'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s d -d 'Stats per device (-B only)'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s q -d 'Be quiet'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s r -d 'Read only mode'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s R -d 'Raw print mode, print full data instead of summary'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -l limit -d 'Set the scrub throughput limit'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s c -d 'Set ioprio class (see ionice(1) manpage)'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s n -d 'Set ioprio classdata (see ionice(1) manpage)'
complete -f -c btrfs -n '__btrfs_command_groups scrub start' -s f -d 'Force starting new scrub'
# btrfs scrub resume
complete -f -c btrfs -n '__btrfs_command_groups scrub resume' -s B -d 'Do not background'
complete -f -c btrfs -n '__btrfs_command_groups scrub resume' -s d -d 'Stats per device (-B only)'
complete -f -c btrfs -n '__btrfs_command_groups scrub resume' -s q -d 'Be quiet'
complete -f -c btrfs -n '__btrfs_command_groups scrub resume' -s r -d 'Read only mode'
complete -f -c btrfs -n '__btrfs_command_groups scrub resume' -s R -d 'Raw print mode, print full data instead of summary'
complete -f -c btrfs -n '__btrfs_command_groups scrub resume' -s c -d 'Set ioprio class (see ionice(1) manpage)'
complete -f -c btrfs -n '__btrfs_command_groups scrub resume' -s n -d 'Set ioprio classdata (see ionice(1) manpage)'
# btrfs scrub status
complete -f -c btrfs -n '__btrfs_command_groups scrub status' -s d -d 'Stats per DEVICE'
complete -f -c btrfs -n '__btrfs_command_groups scrub status' -s R -d 'Print raw stats'

# btrfs rescue
complete -f -c btrfs -n $rescue -a chunk-recover -d 'Recover the chunk tree by scanning the devices one by one.'
complete -f -c btrfs -n $rescue -a super-recover -d 'Recover bad superblocks from good copies'
complete -f -c btrfs -n $rescue -a zero-log -d 'Clear the tree log. Usable if it\'s corrupted and prevents mount.'
complete -f -c btrfs -n $rescue -a fix-device-size -d 'Re-align device and super block sizes'
complete -f -c btrfs -n $rescue -a clear-ino-cache -d 'Remove leftover items pertaining to the deprecated inode cache feature'
complete -f -c btrfs -n $rescue -a clear-space-cache -d 'Completely remove the on-disk data of free space cache of given version'
complete -f -c btrfs -n $rescue -a clear-uuid-tree -d 'Clear the UUID tree'
# btrfs rescue chunk-recover
complete -f -c btrfs -n '__btrfs_command_groups rescue chunk-recover' -s y -d 'Assume an answer of YES to all questions'
complete -f -c btrfs -n '__btrfs_command_groups rescue chunk-recover' -s v -d 'Verbose mode'
# btrfs rescue super-recover
complete -f -c btrfs -n '__btrfs_command_groups rescue super-recover' -s y -d 'Assume an answer of YES to all questions'
complete -f -c btrfs -n '__btrfs_command_groups rescue super-recover' -s v -d 'Verbose mode'

# btrfs inspect-internal
complete -f -c btrfs -n $inspect_internal -a inode-resolve -d 'Get file system paths for the given inode'
complete -f -c btrfs -n $inspect_internal -a logical-resolve -d 'Get file system paths for the given logical address'
complete -f -c btrfs -n $inspect_internal -a subvolid-resolve -d 'Get file system paths for the given subvolume ID.'
complete -f -c btrfs -n $inspect_internal -a rootid -d 'Get tree ID of the containing subvolume of path.'
complete -f -c btrfs -n $inspect_internal -a min-dev-size -d 'Get the minimum size the device can be shrunk to. (Default: 1)'
complete -f -c btrfs -n $inspect_internal -a dump-tree -d 'Dump tree structures from a given device'
complete -f -c btrfs -n $inspect_internal -a dump-super -d 'Dump superblock from a device in a textual form'
complete -f -c btrfs -n $inspect_internal -a tree-stats -d 'Print various stats for trees'
complete -f -c btrfs -n $inspect_internal -a list-chunks -d 'Enumerate chunks on all devices'
complete -f -c btrfs -n $inspect_internal -a map-swapfile -d 'Find device-specific physical offset of file that can be used for hibernation'
# btrfs inspect-internal inode-resolve
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal inode-resolve' -s v -d 'Verbose mode'
# btrfs inspect-internal logical-resolve
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal logical-resolve' -s P -d 'Skip the path resolving and print the inodes instead'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal logical-resolve' -s v -d 'Verbose mode'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal logical-resolve' -s s -d 'Set inode container\'s SIZE'
# btrfs inspect-internal min-dev-size
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal min-dev-size' -l id -d 'Specify the DEVICE-ID to query'
# btrfs inspect-internal dump-tree
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -s e -l extents -d 'Print only extent info: extent and device trees'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -s d -l device -d 'Print only device info: tree root, chunk and device trees'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -s r -l roots -d 'Print only short root node info'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -s R -l backups -d 'Print short root node info and backup root info'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -s u -l uuid -d 'Print only the uuid tree'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -s b -l block -r -d 'Print info from the specified BLOCK only'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -s t -l tree -r -d 'Print only tree with the given ID'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -l follow -d 'Use with -b, to show all children tree blocks of <block_num>'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -l noscan -d 'Do not scan the devices from the filesystem'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -l bfs -d 'Breadth-first traversal of the trees, print nodes, then leaves'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -l dfs -d 'Depth-first traversal of the trees'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -l hide-names -d 'Print placeholder instead of names'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -l csum-headers -d 'Print b-tree node checksums in headers'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-tree' -l csum-items -d 'Print checksums stored in checksum items'

# btrfs inspect-internal dump-super
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-super' -s f -l full -d 'Print full superblock information, backup roots etc.'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-super' -s a -l all -d 'Print information about all superblocks'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-super' -s s -l super -d 'Specify which SUPER-BLOCK copy to print out' -ra '{0,1,2}'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-super' -s F -l force -d 'Attempt to dump superblocks with bad magic'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal dump-super' -l bytenr -d 'Specify alternate superblock OFFSET'
# btrfs inspect-internal logical-resolve
complete -f -c btrfs -n '__btrfs_command_groups logical-resolve' -s P -d 'Print inodes instead of resolving paths'
complete -f -c btrfs -n '__btrfs_command_groups logical-resolve' -s o -d 'Ignore offsets, find all references to an extent'
complete -f -c btrfs -n '__btrfs_command_groups logical-resolve' -s s -r -d 'Set internal buffer size for storing file names'
# btrfs inspect-internal tree-stats
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -s b -d 'Show raw numbers in bytes'
# btrfs inspect-internal list-chunks
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l sort -ra 'devid pstart lstart usage length' -d 'Sort by a column (ascending)'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal list-chunks' -l tbytes -d 'Show sizes in TiB, or TB with --si'
# btrfs inspect-internal map-swapfile
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal map-swapfile' -l resume-offset -s r -d 'Print the value suitable as resume offset for /sys/power/resume_offset.'
# btrfs inspect-internal tree-stats
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -s t -r -d 'Print stats only for the given treeid'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l raw -s b -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups inspect-internal tree-stats' -l tbytes -d 'Show sizes in TiB, or TB with --si'

# btrfs property
complete -f -c btrfs -n $property -a get -d 'Get a property value of a btrfs object'
complete -f -c btrfs -n $property -a set -d 'Set a property on a btrfs object'
complete -f -c btrfs -n $property -a list -d 'Lists available properties with their descriptions for the given object'
# btrfs property get
complete -f -c btrfs -n '__btrfs_command_groups property get' -s t -d 'List properties for the given object type' -ra '{inode,subvol,filesystem,device}'
# btrfs property set
complete -f -c btrfs -n '__btrfs_command_groups property set' -s t -d 'List properties for the given object type' -ra '{inode,subvol,filesystem,device}'
# btrfs property list
complete -f -c btrfs -n '__btrfs_command_groups property list' -s t -d 'List properties for the given object type' -ra '{inode,subvol,filesystem,device}'

# btrfs quota
complete -f -c btrfs -n $quota -a enable -d 'Enable subvolume quota support for a filesystem.'
complete -f -c btrfs -n $quota -a disable -d 'Disable subvolume quota support for a filesystem.'
complete -f -c btrfs -n $quota -a rescan -d 'Trash all qgroup numbers and scan the metadata again with the current config.'
# btrfs quota enable
complete -f -c btrfs -n '__btrfs_command_groups quota enable' -s s -l simple -d 'Use simple quotas'
# btrfs quota rescan
complete -f -c btrfs -n '__btrfs_command_groups quota rescan' -s s -l status -d 'Show status of a running rescan operation'
complete -f -c btrfs -n '__btrfs_command_groups quota rescan' -s w -l wait -d 'Wait for rescan operation to finish'
complete -f -c btrfs -n '__btrfs_command_groups quota rescan' -s W -l wait-norescan -d 'Wait for rescan to finish without starting it'

# btrfs qgroup
complete -f -c btrfs -n $qgroup -a assign -d 'Assign SRC as the child qgroup of DST'
complete -f -c btrfs -n $qgroup -a remove -d 'Remove a child qgroup SRC from DST.'
complete -f -c btrfs -n $qgroup -a create -d 'Create a subvolume quota group.'
complete -f -c btrfs -n $qgroup -a destroy -d 'Destroy a quota group.'
complete -f -c btrfs -n $qgroup -a show -d 'Show subvolume quota groups.'
complete -f -c btrfs -n $qgroup -a clear-stale -d 'Clear all stale qgroups whose subvolume does not exist'
complete -f -c btrfs -n $qgroup -a limit -d 'Set the limits a subvolume quota group.'
# btrfs qgroup assign
complete -f -c btrfs -n '__btrfs_command_groups qgroup assign' -l rescan -d 'Schedule qutoa rescan if needed'
complete -f -c btrfs -n '__btrfs_command_groups qgroup assign' -l no-rescan -d 'Don\'t schedule quota rescan'
# btrfs qgroup show
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -s p -d 'Print parent qgroup id'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -s c -d 'Print child qgroup id'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -s r -d 'Print limit of referenced size of qgroup'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -s e -d 'Print limit of exclusive size of qgroup'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -s F -d 'List all qgroups including ancestral qgroups'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -s f -d 'List all qgroups excluding ancestral qgroups'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l raw -d 'Show raw numbers in bytes'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l human-readable -d 'Show human friendly numbers, base 1024'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l iec -d 'Use 1024 as a base (KiB, MiB, GiB, TiB)'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l si -d 'Use 1000 as a base (kB, MB, GB, TB)'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l kbytes -d 'Show sizes in KiB, or kB with --si'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l mbytes -d 'Show sizes in MiB, or MB with --si'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l gbytes -d 'Show sizes in GiB, or GB with --si'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l tbytes -d 'Show sizes in TiB, or TB with --si'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l sort -d 'List qgroups sorted by specified items' -a '{qgroupid,rfer,excl,max_rfer,max_excl}'
complete -f -c btrfs -n '__btrfs_command_groups qgroup show' -l sync -d 'Force sync of the filesystem before getting info'
# btrfs qgroup limit
complete -f -c btrfs -n '__btrfs_command_groups qgroup limit' -s c -d 'Limit amount of data after compression'
complete -f -c btrfs -n '__btrfs_command_groups qgroup limit' -s e -d 'Limit space exclusively assigned to this qgroup'

# btrfs replace
complete -f -c btrfs -n $replace -a start -d 'Replace device of a btrfs filesystem.'
complete -f -c btrfs -n $replace -a status -d 'Print status and progress information of a running device replace'
complete -f -c btrfs -n $replace -a cancel -d 'Cancel a running device replace operation.'
# btrfs replace start
complete -f -c btrfs -n '__btrfs_command_groups replace start' -s r -d 'Only read from <srcdev> if no other zero-defect mirror exists'
complete -f -c btrfs -n '__btrfs_command_groups replace start' -s f -d 'Force using and overwriting <targetdev>'
complete -f -c btrfs -n '__btrfs_command_groups replace start' -s B -d 'Do not background'
complete -f -c btrfs -n '__btrfs_command_groups replace start' -l enqueue -d "Wait if there's another exclusive operation running"
complete -f -c btrfs -n '__btrfs_command_groups replace start' -s K -l nodiscard -d 'Do not perform TRIM on DEVICES'
# btrfs replace status
complete -f -c btrfs -n '__btrfs_command_groups replace status' -s 1 -d 'Only print once until the replace operation finishes'
