# Fish completions for the OpenZFS zfs command
# TODO Possible enhancements:
# - add a test to propose iSCSI and Trusted Extensions completions only when such system is present;
# - Illumos man pages suggests that it does not support nbmand nor atime mount option, so these properties should be proposed only when available
# - generally, propose properties only when the current OS and ZFS versions support them;
# - for the promote command, propose only eligible filesystems;
# - for the rollback command, propose only the most recent snapshot for each dataset, as it will not accept an intermediary snapshot;
# - for the release command, complete with existing tags;
# - for the diff command, complete the destination dataset of the diff;
# - for the program command, complete the script to be executed
# - for commands accepting several arguments of different types, propose arguments in the right order: for get, once the ZFS parameters have been given, only propose datasets

set OS ""
switch (uname)
    case Linux
        set OS "Linux"
    case Darwin
        set OS "macOS"
    case FreeBSD
        set OS "FreeBSD"
    case SunOS
        set OS "SunOS"
        # Others?
    case "*"
        set OS "unknown"
end

# Does the current invocation need a command?
function __fish_zfs_needs_command
    set -l bookmark ""
    if __fish_is_zfs_feature_enabled "feature@bookmarks"
        set bookmark "bookmark"
    end

    not __fish_seen_subcommand_from \? create destroy snap{,shot} rollback clone promote rename list set get inherit upgrade {user,group}space {u,un,}mount {un,}share $bookmark send receive recv {un,}allow hold{s,} release diff program
end

function __fish_zfs_using_command # ZFS command whose completions are looked for
    set -l cmd (commandline -opc)
    if test (count $cmd) -eq 1 # The token can only be 'zfs', so skip
        return 2
    end
    if test $cmd[2] = $argv
        return 0
    else
        return 1
    end
end

function __fish_zfs_list_dataset_types
    echo -e "filesystem\t"(_ "ZFS filesystem")
    echo -e "snapshot\t"(_ "ZFS filesystem snapshot")
    echo -e "volume\t"(_ "ZFS block storage device")
    echo -e "bookmark\t"(_ "ZFS snapshot bookmark")
    echo -e "all\t"(_ "Any ZFS dataset")
end

function __fish_zfs_list_source_types
    echo -e "local\t"(_ "Dataset-specific value")
    echo -e "default\t"(_ "Default ZFS value")
    echo -e "inherited\t"(_ "Value inherited from parent dataset")
    echo -e "received\t"(_ "Value received by 'zfs receive'")
    echo -e "temporary\t"(_ "Value valid for the current mount")
    echo -e "none\t"(_ "Read-only value")
end

function __fish_zfs_list_get_fields
    echo -e "name\t"(_ "Dataset full name")
    echo -e "property\t"(_ "Property")
    echo -e "value\t"(_ "Property value")
    echo -e "source\t"(_ "Property value origin")
end

function __fish_zfs_list_space_fields
    echo -e "type\t"(_ "Identity type")
    echo -e "name\t"(_ "Identity name")
    echo -e "used\t"(_ "Space usage")
    echo -e "quota\t"(_ "Space quota")
end

function __fish_zfs_list_userspace_types
    echo -e "posixuser\t"(_ "POSIX user")
    echo -e "smbuser\t"(_ "Samba user")
    echo -e "all\t"(_ "Both types")
end

function __fish_zfs_list_groupspace_types
    echo -e "posixgroup\t"(_ "POSIX group")
    echo -e "smbgroup\t"(_ "Samba group")
    echo -e "all\t"(_ "Both types")
end

function __fish_zfs_list_permissions
    echo -e "allow\t"(_ "Also needs the permission to be allowed")
    echo -e "clone\t"(_ "Also needs the 'create' and 'mount' permissions in the origin filesystem")
    echo -e "create\t"(_ "Also needs the 'mount' permission")
    echo -e "destroy\t"(_ "Also needs the 'mount' permission")
    echo -e "mount"
    echo -e "promote\t"(_ "Also needs the 'promote' and 'mount' permissions in the origin filesystem")
    echo -e "receive\t"(_ "Also needs the 'create' and 'mount' permissions")
    echo -e "rename\t"(_ "Also needs the 'create' and 'mount' permissions in the new parent")
    echo -e "rollback\t"(_ "Also needs the 'mount' permission")
    echo -e "send"
    echo -e "share\t"(_ "For SMB and NFS shares")
    echo -e "snapshot\t"(_ "Also needs the 'mount' permission")
    echo -e "groupquota"
    echo -e "groupused"
    echo -e "userprop\t"(_ "Allows changing any user property")
    echo -e "userquota"
    echo -e "userused"
    # The remaining code of the function is almost a duplicate of __fish_complete_zfs_rw_properties and __fish_complete_zfs_ro_properties, but almost only, hence the duplication
    # RO properties
    echo -e "volblocksize\t"(_ "Volume block size")
    # R/W properties
    echo -e "aclinherit\t"(_ "Inheritance of ACL entries")" (discard, noallow, restricted, passthrough, passthrough-x)"
    echo -e "atime\t"(_ "Update access time on read")" (on, off)"
    echo -e "canmount\t"(_ "Is the dataset mountable")" (on, off, noauto)"
    set -l additional_cksum_algs ''
    set -l additional_dedup_algs ''
    if contains -- $OS FreeBSD SunOS
        set additional_cksum_algs ", noparity"
    end
    if __fish_is_zfs_feature_enabled "feature@sha512"
        set additional_cksum_algs "$additional_cksum_algs, sha512"
        set additional_dedup_algs ", sha512[,verify]"
    end
    if __fish_is_zfs_feature_enabled "feature@skein"
        set additional_cksum_algs "$additional_cksum_algs, skein"
        set additional_dedup_algs "$additional_dedup_algs, skein[,verify]"
    end
    if __fish_is_zfs_feature_enabled "feature@edonr"
        set additional_cksum_algs "$additional_cksum_algs, edonr"
        set additional_dedup_algs "$additional_dedup_algs, edonr[,verify]"
    end
    echo -e "checksum\t"(_ "Data checksum")" (on, off, fletcher2, fletcher4, sha256$additional_cksum_algs)"
    set -e additional_cksum_algs
    set -l additional_compress_algs ''
    if __fish_is_zfs_feature_enabled "feature@lz4_compress"
        set additional_compress_algs ", lz4"
    end
    echo -e "compression\t"(_ "Compression algorithm")" (on, off, lzjb$additional_compress_algs, gzip, gzip-[1-9], zle)"
    set -e additional_compress_algs
    echo -e "copies\t"(_ "Number of copies of data")" (1, 2, 3)"
    echo -e "dedup\t"(_ "Deduplication")" (on, off, verify, sha256[,verify]$additional_dedup_algs)"
    set -e additional_dedup_algs
    echo -e "devices\t"(_ "Are contained device nodes openable")" (on, off)"
    echo -e "exec\t"(_ "Can contained executables be executed")" (on, off)"
    echo -e "filesystem_limit\t"(_ "Max number of filesystems and volumes")" (COUNT, none)"
    echo -e "mountpoint\t"(_ "Mountpoint")" (PATH, none, legacy)"
    echo -e "primarycache\t"(_ "Which data to cache in ARC")" (all, none, metadata)"
    echo -e "quota\t"(_ "Max size of dataset and children")" (SIZE, none)"
    echo -e "snapshot_limit\t"(_ "Max number of snapshots")" (COUNT, none)"
    echo -e "readonly\t"(_ "Read-only")" (on, off)"
    echo -e "recordsize\t"(_ "Suggest block size")" (SIZE)"
    echo -e "redundant_metadata\t"(_ "How redundant are the metadata")" (all, most)"
    echo -e "refquota\t"(_ "Max space used by dataset itself")" (SIZE, none)"
    echo -e "refreservation\t"(_ "Min space guaranteed to dataset itself")" (SIZE, none)"
    echo -e "reservation\t"(_ "Min space guaranteed to dataset")" (SIZE, none)"
    echo -e "secondarycache\t"(_ "Which data to cache in L2ARC")" (all, none, metadata)"
    echo -e "setuid\t"(_ "Respect set-UID bit")" (on, off)"
    echo -e "sharenfs\t"(_ "Share in NFS")" (on, off, OPTS)"
    echo -e "logbias\t"(_ "Hint for handling of synchronous requests")" (latency, throughput)"
    echo -e "snapdir\t"(_ "Hide .zfs directory")" (hidden, visible)"
    echo -e "sync\t"(_ "Handle of synchronous requests")" (standard, always, disabled)"
    echo -e "volsize\t"(_ "Volume logical size")" (SIZE)"
    if test $OS = "SunOS"
        echo -e "zoned\t"(_ "Managed from a non-global zone")" (on, off)"
        echo -e "mlslabel\t"(_ "Can the dataset be mounted in a zone with Trusted Extensions enabled")" (LABEL, none)"
        echo -e "nbmand\t"(_ "Mount with Non Blocking mandatory locks")" (on, off)"
        echo -e "sharesmb\t"(_ "Share in Samba")" (on, off)"
        echo -e "shareiscsi\t"(_ "Share as an iSCSI target")" (on, off)"
        echo -e "version\t"(_ "On-disk version of filesystem")" (1, 2, current)"
        echo -e "vscan\t"(_ "Scan regular files for viruses on opening and closing")" (on, off)"
        echo -e "xattr\t"(_ "Extended attributes")" (on, off, sa)"
    else if test $OS = "Linux"
        echo -e "acltype\t"(_ "Use no ACL or POSIX ACL")" (noacl, posixacl)"
        echo -e "nbmand\t"(_ "Mount with Non Blocking mandatory locks")" (on, off)"
        echo -e "relatime\t"(_ "Sometimes update access time on read")" (on, off)"
        echo -e "shareiscsi\t"(_ "Share as an iSCSI target")" (on, off)"
        echo -e "sharesmb\t"(_ "Share in Samba")" (on, off)"
        echo -e "snapdev\t"(_ "Hide volume snapshots")" (hidden, visible)"
        echo -e "version\t"(_ "On-disk version of filesystem")" (1, 2, current)"
        echo -e "vscan\t"(_ "Scan regular files for viruses on opening and closing")" (on, off)"
        echo -e "xattr\t"(_ "Extended attributes")" (on, off, sa)"
    else if test $OS = "FreeBSD"
        echo -e "aclmode\t"(_ "How is ACL modified by chmod")" (discard, groupmask, passthrough, restricted)"
        echo -e "volmode\t"(_ "How to expose volumes to OS")" (default, geom, dev, none)"
    end
    # Write-once properties
    echo -e "normalization\t"(_ "Unicode normalization")" (none, formC, formD, formKC, formKD)"
    echo -e "utf8only\t"(_ "Reject non-UTF-8-compliant filenames")" (on, off)"
    if test $OS = "Linux"
        echo -e "casesensitivity\t"(_ "Case sensitivity")" (sensitive, insensitive)"
    else # FreeBSD, SunOS and virtually all others
        echo -e "casesensitivity\t"(_ "Case sensitivity")" (sensitive, insensitive, mixed)"
    end
    # Permissions set; if none are found, or if permission sets are not supported, no output is expected, even an error
    for i in (zpool list -o name -H)
        zfs allow $i
    end | string match -r '@[[:alnum:]]*' | sort -u
end

function __fish_print_zfs_bookmarks -d "Lists ZFS bookmarks, if the feature is enabled"
    if __fish_is_zfs_feature_enabled 'feature@bookmarks'
        zfs list -t bookmark -o name -H
    end
end

function __fish_print_zfs_filesystems -d "Lists ZFS filesystems"
    zfs list -t filesystem -o name -H
end

function __fish_print_zfs_volumes -d "Lists ZFS volumes"
    zfs list -t volume -o name -H
end

complete -c zfs -f -n '__fish_zfs_needs_command' -s '?' -a '?' -d 'Display a help message'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'create' -d 'Create a volume or filesystem'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'destroy' -d 'Destroy a dataset'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'snapshot snap' -d 'Create a snapshot'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'rollback' -d 'Roll back a filesystem to a previous snapshot'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'clone' -d 'Create a clone of a snapshot'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'promote' -d 'Promotes a clone file system to no longer be dependent on its "origin" snapshot'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'rename' -d 'Rename a dataset'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'list' -d 'List dataset properties'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'set' -d 'Set a dataset property'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'get' -d 'Get one or several dataset properties'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'inherit' -d 'Set a dataset property to be inherited'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'upgrade' -d 'List upgradeable datasets, or upgrade one'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'userspace' -d 'Get dataset space consumed by each user'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'groupspace' -d 'Get dataset space consumed by each user group'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'mount' -d 'Mount a filesystem'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'unmount umount' -d 'Unmount a filesystem'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'share' -d 'Share a given filesystem, or all of them'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'unshare' -d 'Stop sharing a given filesystem, or all of them'
if __fish_is_zfs_feature_enabled 'feature@bookmarks'
    complete -c zfs -f -n '__fish_zfs_needs_command' -a 'bookmark' -d 'Create a bookmark for a snapshot'
end
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'send' -d 'Output a stream representation of a dataset'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'receive recv' -d 'Write on disk a dataset from the stream representation given on standard input'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'allow' -d 'Delegate, or display delegations of, rights on a dataset to (groups of) users'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'unallow' -d 'Revoke delegations of rights on a dataset from (groups of) users'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'hold' -d 'Put a named hold on a snapshot'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'holds' -d 'List holds on a snapshot'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'release' -d 'Remove a named hold from a snapshot'
complete -c zfs -f -n '__fish_zfs_needs_command' -a 'diff' -d 'List changed files between a snapshot, and a filesystem or a previous snapshot'
if test $OS = 'SunOS' # This is currently only supported under Illumos, but that will probably change
    complete -c zfs -f -n '__fish_zfs_needs_command' -a 'program' -d 'Execute a ZFS Channel Program'
end

# Completions hereafter try to follow the man pages commands order, for maintainability, at the cost of multiple if statements

# create completions
complete -c zfs -f -n '__fish_zfs_using_command create' -s p -d 'Create all needed non-existing parent datasets'
if test $OS = 'Linux' # Only Linux supports the comma-separated format; others need multiple -o calls
    complete -c zfs -x -n '__fish_zfs_using_command create' -s o -d 'Dataset property' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties))'
else
    complete -c zfs -x -n '__fish_zfs_using_command create' -s o -d 'Dataset property' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties)'
end
# create completions for volumes; as -V is necessary to zfs to recognize a volume creation request, we use it as a condition to propose volume creation completions
# If -V is typed after -s or -b, zfs should accept it, but fish won't propose -s or -b, but, as these options are for volumes only, it seems reasonable to expect the user to ask for a volume, with -V, before giving its characteristics with -s or -b
complete -c zfs -x -n '__fish_zfs_using_command create' -s V -d 'Volume size'
complete -c zfs -f -n '__fish_zfs_using_command create; and __fish_contains_opt -s V' -s s -d 'Create a sparse volume'
complete -c zfs -x -n '__fish_zfs_using_command create; and __fish_contains_opt -s V' -s b -d 'Blocksize'

# destroy completions; as the dataset is the last item, we can't know yet if it's a snapshot, a bookmark or something else, so we can't separate snapshot-specific options from others
complete -c zfs -f -n '__fish_zfs_using_command destroy' -s r -d 'Recursively destroy children'
complete -c zfs -f -n '__fish_zfs_using_command destroy' -s R -d 'Recursively destroy all dependents'
complete -c zfs -f -n '__fish_zfs_using_command destroy' -s f -d 'Force unmounting'
complete -c zfs -f -n '__fish_zfs_using_command destroy' -s n -d 'Dry run'
complete -c zfs -f -n '__fish_zfs_using_command destroy' -s p -d 'Print machine-parsable verbose information'
complete -c zfs -f -n '__fish_zfs_using_command destroy' -s v -d 'Verbose mode'
complete -c zfs -f -n '__fish_zfs_using_command destroy' -s d -d 'Defer snapshot deletion'
complete -c zfs -x -n '__fish_zfs_using_command destroy' -d 'Dataset to destroy' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots; __fish_print_zfs_bookmarks)'

# snapshot completions
complete -c zfs -f -n '__fish_zfs_using_command snapshot; or __fish_zfs_using_command snap' -s r -d 'Recursively snapshot children'
if test $OS = 'Linux' # Only Linux supports the comma-separated format; others need multiple -o calls
    complete -c zfs -x -n '__fish_zfs_using_command snapshot; or __fish_zfs_using_command snap' -s o -d 'Snapshot property' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties))'
else
    complete -c zfs -x -n '__fish_zfs_using_command snapshot; or __fish_zfs_using_command snap' -s o -d 'Snapshot property' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties)'
end
complete -c zfs -x -n '__fish_zfs_using_command snapshot; or __fish_zfs_using_command snap' -d 'Dataset to snapshot' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes)'

# rollback completions
complete -c zfs -f -n '__fish_zfs_using_command rollback' -s r -d 'Destroy later snapshot and bookmarks'
complete -c zfs -f -n '__fish_zfs_using_command rollback' -s R -d 'Recursively destroy later snapshot, bookmarks and clones'
complete -c zfs -f -n '__fish_zfs_using_command rollback; and __fish_contains_opt -s R' -s f -d 'Force unmounting of clones'
complete -c zfs -x -n '__fish_zfs_using_command rollback' -d 'Snapshot to roll back to' -a '(__fish_print_zfs_snapshots)'

# clone completions
complete -c zfs -f -n '__fish_zfs_using_command clone' -s p -d 'Create all needed non-existing parent datasets'
if test $OS = 'Linux' # Only Linux supports the comma-separated format; others need multiple -o calls
    complete -c zfs -x -n '__fish_zfs_using_command clone' -s o -d 'Clone property' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties))'
else
    complete -c zfs -x -n '__fish_zfs_using_command clone' -s o -d 'Clone property' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties)'
end
complete -c zfs -x -n '__fish_zfs_using_command clone' -d 'Snapshot to clone' -a '(__fish_print_zfs_snapshots)'

# promote completions
complete -c zfs -x -n '__fish_zfs_using_command promote' -d 'Clone to promote' -a '(__fish_print_zfs_filesystems)'

# rename completions; as the dataset is the last item, we can't know yet if it's a snapshot or not, we can't separate snapshot-specific option from others
complete -c zfs -f -n '__fish_zfs_using_command rename' -s p -d 'Create all needed non-existing parent datasets'
complete -c zfs -f -n '__fish_zfs_using_command rename' -s r -d 'Recursively rename children snapshots'
if test $OS = 'Linux'
    complete -c zfs -f -n '__fish_zfs_using_command rename' -s f -d 'Force unmounting if needed'
else if test $OS = 'FreeBSD'
    complete -c zfs -f -n '__fish_zfs_using_command rename' -s u -d 'Do not remount filesystems during rename'
    complete -c zfs -f -n '__fish_zfs_using_command rename; and __fish_not_contain_opt -s u' -s f -d 'Force unmounting if needed'
end
complete -c zfs -x -n '__fish_zfs_using_command rename' -d 'Dataset to rename' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# list completions
complete -c zfs -f -n '__fish_zfs_using_command list' -s H -d 'Print output in a machine-parsable format'
complete -c zfs -f -n '__fish_zfs_using_command list' -s r -d 'Operate recursively on datasets'
complete -c zfs -x -n '__fish_zfs_using_command list; and __fish_contains_opt -s r' -s d -d 'Maximum recursion depth'
complete -c zfs -x -n '__fish_zfs_using_command list' -s o -d 'Property to list' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo -e "name\t"(_ "Dataset name")"; echo -e "space\t"(_ "Space properties")"))'
complete -c zfs -f -n '__fish_zfs_using_command list' -s p -d 'Print parsable (exact) values for numbers'
complete -c zfs -x -n '__fish_zfs_using_command list; and __fish_not_contain_opt -s S' -s s -d 'Property to use for sorting output by ascending order' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo -e "name\t"(_ "Dataset name"))'
complete -c zfs -x -n '__fish_zfs_using_command list; and __fish_not_contain_opt -s s' -s S -d 'Property to use for sorting output by descending order' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo -e "name\t"(_ "Dataset name"))'
complete -c zfs -x -n '__fish_zfs_using_command list' -s t -d 'Dataset type' -a '(__fish_zfs_list_dataset_types)'
complete -c zfs -x -n '__fish_zfs_using_command list' -d 'Dataset which properties is to be listed' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# set completions
complete -c zfs -x -n '__fish_zfs_using_command set' -d 'Property to set' -a '(__fish_complete_zfs_rw_properties)'
complete -c zfs -x -n '__fish_zfs_using_command set; and string match -q -r "zfs set \S+ " (commandline -c)' -d 'Dataset which property is to be setted' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# get completions
complete -c zfs -f -n '__fish_zfs_using_command get' -s r -d 'Operate recursively on datasets'
complete -c zfs -x -n '__fish_zfs_using_command get; and __fish_contains_opt -s r' -s d -d 'Maximum recursion depth'
complete -c zfs -f -n '__fish_zfs_using_command get' -s H -d 'Print output in a machine-parsable format'
complete -c zfs -x -n '__fish_zfs_using_command get' -s o -d 'Fields to display' -a '(__fish_append , (__fish_zfs_list_get_fields))'
complete -c zfs -x -n '__fish_zfs_using_command get' -s s -d 'Property source to display' -a '(__fish_append , (__fish_zfs_list_source_types))'
complete -c zfs -f -n '__fish_zfs_using_command get' -s p -d 'Print parsable (exact) values for numbers'
complete -c zfs -x -n '__fish_zfs_using_command get' -s t -d 'Dataset type' -a '(__fish_append , (__fish_zfs_list_dataset_types))'
complete -c zfs -x -n '__fish_zfs_using_command get' -d 'Property to get' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo "all"))'
complete -c zfs -x -n '__fish_zfs_using_command get' -d 'Dataset which properties is to be got' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# inherit completions
complete -c zfs -f -n '__fish_zfs_using_command inherit' -s r -d 'Operate recursively on datasets'
complete -c zfs -f -n '__fish_zfs_using_command inherit' -s S -d 'Revert to the received value if available'
complete -c zfs -x -n '__fish_zfs_using_command inherit' -d 'Dataset which properties is to be inherited' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# upgrade completions
complete -c zfs -f -n '__fish_zfs_using_command upgrade; and __fish_not_contain_opt -s a -s r -s V' -s v -d 'Verbose mode'
complete -c zfs -f -n '__fish_zfs_using_command upgrade; and __fish_not_contain_opt -s v' -s a -d 'Upgrade all eligible filesystems'
complete -c zfs -f -n '__fish_zfs_using_command upgrade; and __fish_not_contain_opt -s v' -s r -d 'Operate recursively on datasets'
complete -c zfs -x -n '__fish_zfs_using_command upgrade; and __fish_not_contain_opt -s v' -s V -d 'Upgrade to the specified version'
complete -c zfs -x -n '__fish_zfs_using_command upgrade; and __fish_not_contain_opt -s a' -d 'Filesystem to upgrade' -a '(__fish_print_zfs_filesystems)'

# userspace/groupspace completions
complete -c zfs -f -n '__fish_zfs_using_command userspace' -s n -d 'Print UID instead of user name'
complete -c zfs -f -n '__fish_zfs_using_command groupspace' -s n -d 'Print GID instead of group name'
complete -c zfs -f -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace' -s H -d 'Print output in a machine-parsable format'
complete -c zfs -f -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace' -s p -d 'Print parsable (exact) values for numbers'
complete -c zfs -x -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace' -s o -d 'Field to display' -a '(__fish_append , (__fish_zfs_list_space_fields))'
complete -c zfs -x -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace; and __fish_not_contain_opt -s S' -s s -d 'Property to use for sorting output by ascending order' -a '__fish_zfs_list_space_fields'
complete -c zfs -x -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace; and __fish_not_contain_opt -s s' -s S -d 'Property to use for sorting output by descending order' -a '__fish_zfs_list_space_fields'
complete -c zfs -x -n '__fish_zfs_using_command userspace' -s t -d 'Identity types to display' -a '(__fish_append , (__fish_zfs_list_userspace_types))'
complete -c zfs -x -n '__fish_zfs_using_command groupspace' -s t -d 'Identity types to display' -a '(__fish_append , (__fish_zfs_list_groupspace_types))'
complete -c zfs -f -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace' -s i -d 'Translate S(amba)ID to POSIX ID'
complete -c zfs -x -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace' -d 'Dataset which space usage is to be got' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_snapshots)'

# mount completions
complete -c zfs -x -n '__fish_zfs_using_command mount' -s o -d 'Temporary mount point property' -a '(__fish_append , (__fish_complete_zfs_mountpoint_properties))'
complete -c zfs -f -n '__fish_zfs_using_command mount' -s v -d 'Report progress'
complete -c zfs -f -n '__fish_zfs_using_command mount' -s a -d 'Mount all available ZFS filesystems'
if contains -- $OS Linux SunOS
    complete -c zfs -f -n '__fish_zfs_using_command mount' -s O -d 'Overlay mount'
end
complete -c zfs -x -n '__fish_zfs_using_command mount; and __fish_not_contain_opt -s a' -d 'Filesystem to mount' -a '(__fish_print_zfs_filesystems)'

# unmount completions
complete -c zfs -f -n '__fish_zfs_using_command unmount; or __fish_zfs_using_command umount' -s f -d 'Force unmounting if needed'
complete -c zfs -f -n '__fish_zfs_using_command unmount; or __fish_zfs_using_command umount' -s a -d 'Unmount all available ZFS filesystems'
complete -c zfs -x -n '__fish_zfs_using_command unmount; or __fish_zfs_using_command umount; and __fish_not_contain_opt -s a' -d 'Filesystem to unmount' -a '(__fish_print_zfs_filesystems; __fish_print_mounted)'

# share completions
complete -c zfs -f -n '__fish_zfs_using_command share' -s a -d 'Share all eligible ZFS filesystems'
complete -c zfs -x -n '__fish_zfs_using_command share; and __fish_not_contain_opt -s a' -d 'Filesystem to share' -a '(__fish_print_zfs_filesystems)'

# unshare completions
complete -c zfs -f -n '__fish_zfs_using_command unshare' -s a -d 'Unshare all shared ZFS filesystems'
complete -c zfs -x -n '__fish_zfs_using_command unshare; and __fish_not_contain_opt -s a' -d 'Filesystem to unshare' -a '(__fish_print_zfs_filesystems; __fish_print_mounted)'

# bookmark completions
if __fish_is_zfs_feature_enabled 'feature@bookmarks'
    complete -c zfs -x -n '__fish_zfs_using_command bookmark' -d 'Snapshot to bookmark' -a '(__fish_print_zfs_snapshots)'
end

# send completions
complete -c zfs -x -n '__fish_zfs_using_command send' -s i -d 'Generate incremental stream from snapshot' -a '(__fish_print_zfs_snapshots; __fish_print_zfs_bookmarks)'
complete -c zfs -x -n '__fish_zfs_using_command send' -s I -d 'Generate incremental stream from snapshot, even with intermediary snapshots' -a '(__fish_print_zfs_snapshots)'
if test $OS = 'SunOS'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s R -l replicate -d 'Include children in replication stream'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s D -l dedup -d 'Generate a deduplicated stream'
    complete -c zfs -f -n '__fish_zfs_using_command send; and __fish_is_zfs_feature_enabled "feature@large_blocks"' -s L -l large-block -d 'Allow the presence of larger blocks than 128 kiB'
    complete -c zfs -f -n '__fish_zfs_using_command send; and __fish_is_zfs_feature_enabled "feature@embedded_data"' -s e -l embed -d 'Generate a more compact stream by using WRITE_EMBEDDED records'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s P -l parsable -d 'Print verbose information about the stream in a machine-parsable format'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s c -l compressed -d 'Generate compressed stream'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s n -l dryrun -d 'Dry run'
    complete -c zfs -f -n '__fish_zfs_using_command send; and __fish_not_contain_opt -s R' -s p -l props -d 'Include dataset properties'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s v -l verbose -d 'Print verbose information about the stream'
else
    complete -c zfs -f -n '__fish_zfs_using_command send' -s R -d 'Include children in replication stream'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s D -d 'Generate a deduplicated stream'
    complete -c zfs -f -n '__fish_zfs_using_command send; and __fish_is_zfs_feature_enabled "feature@large_blocks"' -s L -d 'Allow the presence of larger blocks than 128 kiB'
    complete -c zfs -f -n '__fish_zfs_using_command send; and __fish_is_zfs_feature_enabled "feature@embedded_data"' -s e -d 'Generate a more compact stream by using WRITE_EMBEDDED records'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s P -d 'Print verbose information about the stream in a machine-parsable format'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s n -d 'Dry run'
    complete -c zfs -f -n '__fish_zfs_using_command send; and __fish_not_contain_opt -s R' -s p -d 'Include dataset properties'
    complete -c zfs -f -n '__fish_zfs_using_command send' -s v -d 'Print verbose information about the stream'
end
complete -c zfs -x -n '__fish_zfs_using_command send' -d 'Dataset to send' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# receive completions
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s v -d 'Print verbose information about the stream and the time spent processing it'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s n -d 'Dry run: do not actually receive the stream'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s F -d 'Force rollback to the most recent snapshot'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s u -d 'Unmount the target filesystem'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv; and __fish_not_contain_opt -s e' -s d -d 'Discard the first element of the path of the sent snapshot'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv; and __fish_not_contain_opt -s d' -s e -d 'Discard all but the last element of the path of the sent snapshot'
if test $OS = "SunOS"
    complete -c zfs -x -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s o -d 'Force the stream to be received as a clone of the given snapshot' -a 'origin'
end
if __fish_is_zfs_feature_enabled "feature@extensible_dataset"
    complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s s -d 'On transfer interruption, store a receive_resume_token for resumption'
end
complete -c zfs -x -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -d 'Dataset to receive' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# allow completions
complete -c zfs -f -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s d' -s l -d 'Delegate permissions only on the specified dataset'
complete -c zfs -f -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s l' -s d -d 'Delegate permissions only on the descendents dataset'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s e' -s u -d 'User to delegate permissions to' -a '(__fish_append , (__fish_complete_users))'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s e' -s g -d 'Group to delegate permissions to' -a '(__fish_append , (__fish_complete_groups))'
if contains -- $OS SunOS FreeBSD
    complete -c zfs -f -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s u -s g -s e' -a 'everyone' -d 'Delegate permission to everyone'
end
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s u -s g everyone' -s e -d 'Delegate permission to everyone'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s s' -s c -d 'Delegate permissions only to the creator of later descendent datasets' -a '(__fish_zfs_append , (__fish_zfs_list_permissions))'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s c' -s s -d 'Create a permission set or add permissions to an existing one'
complete -c zfs -x -n '__fish_zfs_using_command allow' -d 'Dataset on which delegation is to be applied' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes)'

# unallow completions
complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s d' -s l -d 'Remove permissions only on the specified dataset'
complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s l' -s d -d 'Remove permissions only on the descendents dataset'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s e' -s u -d 'User to remove permissions from' -a '(__fish_zfs_append , (__fish_complete_users))'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s e' -s g -d 'Group to remove permissions from' -a '(__fish_zfs_append , (__fish_complete_groups))'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s u -s g everyone' -s e -d 'Remove permission from everyone'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s s' -s c -d 'Remove permissions only on later created descendent datasets' -a '(__fish_zfs_append , (__fish_zfs_list_permissions))'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s c' -s s -d 'Remove a permission set or remove permissions from an existing one'
if test $OS = 'SunOS'
    complete -c zfs -f -n '__fish_zfs_using_command unallow' -s r -d 'Remove permissions recursively'
    complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s u -s g -s e' -a 'everyone' -d 'Remove permission from everyone'
else if test $OS = 'FreeBSD'
    complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s u -s g -s e' -a 'everyone' -d 'Remove permission from everyone'
end
complete -c zfs -x -n '__fish_zfs_using_command unallow' -d 'Dataset on which delegation is to be removed' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes)'

# hold completions
complete -c zfs -f -n '__fish_zfs_using_command hold' -s r -d 'Apply hold recursively'
complete -c zfs -x -n '__fish_zfs_using_command hold' -d 'Snapshot on which hold is to be applied' -a '(__fish_print_zfs_snapshots)'

# holds completions
complete -c zfs -f -n '__fish_zfs_using_command holds' -s r -d 'List holds recursively'
complete -c zfs -x -n '__fish_zfs_using_command holds' -d 'Snapshot which holds are to be listed' -a '(__fish_print_zfs_snapshots)'

# release completions
complete -c zfs -f -n '__fish_zfs_using_command release' -s r -d 'Release hold recursively'
complete -c zfs -x -n '__fish_zfs_using_command release' -d 'Snapshot from which hold is to be removed' -a '(__fish_print_zfs_snapshots)'

# diff completions
complete -c zfs -f -n '__fish_zfs_using_command diff' -s F -d 'Display file type (à la ls -F)'
complete -c zfs -f -n '__fish_zfs_using_command diff' -s H -d 'Print output in a machine-parsable format'
complete -c zfs -f -n '__fish_zfs_using_command diff' -s t -d 'Display inode change time'
complete -c zfs -x -n '__fish_zfs_using_command diff' -d 'Source snapshot for the diff' -a '(__fish_print_zfs_snapshots)'

# program completions
if test $OS = 'SunOS' # This is currently only supported under Illumos, but that will probably change
    complete -c zfs -x -n '__fish_zfs_using_command program' -s t -d 'Execution timeout'
    complete -c zfs -x -n '__fish_zfs_using_command program' -s t -d 'Execution memory limit'
    complete -c zfs -x -n '__fish_zfs_using_command program' -d 'Pool which program will be executed on' -a '(__fish_complete_zfs_pools)'
end
