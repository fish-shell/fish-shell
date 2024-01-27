# Fish completions for the ZFS `zfs` command
#
# Possible enhancements:
# - Add a test to propose iSCSI and Trusted Extensions completions only when such system is present;
# - Illumos man pages suggests that it does not support nbmand nor atime mount option, so these
#   properties should be proposed only when available;
# - Generally, propose properties only when the current OS and ZFS versions support them;
# - For the promote command, propose only eligible filesystems;
# - For the rollback command, propose only the most recent snapshot for each dataset, as it will not
#   accept an intermediary snapshot;
# - For the release command, complete with existing tags;
# - For the diff command, complete the destination dataset of the diff;
# - For the program command, complete the script to be executed;
# - For commands accepting several arguments of different types, propose arguments in the right
#   order: for get, once the ZFS parameters have been given, only propose datasets.

set -l OS ""
set -l freebsd_version ""
switch (uname)
    case Linux
        set OS Linux
    case Darwin
        set OS macOS
    case FreeBSD
        set OS FreeBSD
        set freebsd_version (uname -U)
    case SunOS
        set OS SunOS
        # Others?
    case "*"
        set OS unknown
end

# Certain functionality is exclusive to platforms using OpenZFS. This used to be just Linux, but it
# now includes FreeBSD 13 and above.
if not type -q __fish_is_openzfs
    function __fish_is_openzfs --inherit-variable freebsd_version --inherit-variable OS
        test $OS = Linux || test $OS = FreeBSD -a $freebsd_version -gt 1300000
    end
end

# Does the current invocation need a command?
function __fish_zfs_needs_command
    set -l bookmark ""
    if __fish_is_zfs_feature_enabled "feature@bookmarks"
        set bookmark bookmark
    end

    not __fish_seen_subcommand_from \? create destroy snap{,shot} rollback clone promote rename list set get inherit upgrade {user,group}space {u,un,}mount {un,}share $bookmark send receive recv {un,}allow hold{s,} release diff program
end

function __fish_zfs_using_command # ZFS command whose completions are looked for
    set -l cmd (commandline -xpc)
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
    echo -e "filesystem\tZFS filesystem"
    echo -e "snapshot\tZFS filesystem snapshot"
    echo -e "volume\tZFS block storage device"
    echo -e "bookmark\tZFS snapshot bookmark"
    echo -e "all\tAny ZFS dataset"
end

function __fish_zfs_list_source_types
    echo -e "local\tDataset-specific value"
    echo -e "default\tDefault ZFS value"
    echo -e "inherited\tValue inherited from parent dataset"
    echo -e "received\tValue received by 'zfs receive'"
    echo -e "temporary\tValue valid for the current mount"
    echo -e "none\tRead-only value"
end

function __fish_zfs_list_get_fields
    echo -e "name\tDataset full name"
    echo -e "property\tProperty"
    echo -e "value\tProperty value"
    echo -e "source\tProperty value origin"
end

function __fish_zfs_list_space_fields
    echo -e "type\tIdentity type"
    echo -e "name\tIdentity name"
    echo -e "used\tSpace usage"
    echo -e "quota\tSpace quota"
end

function __fish_zfs_list_userspace_types
    echo -e "posixuser\tPOSIX user"
    echo -e "smbuser\tSamba user"
    echo -e "all\tBoth types"
end

function __fish_zfs_list_groupspace_types
    echo -e "posixgroup\tPOSIX group"
    echo -e "smbgroup\tSamba group"
    echo -e "all\tBoth types"
end

function __fish_zfs_complete_property_values -a name
    # To make updates easier, instead of using a switch/case here we just copy-and-paste the lines
    # from zfsprops(8) directly, and then parse them when the function is called.

    set all_options "
    aclinherit=discard|noallow|restricted|passthrough|passthrough-x
    aclmode=discard|groupmask|passthrough|restricted
    acltype=off|nfsv4|posix
    atime=on|off
    canmount=on|off|noauto
    checksum=on|off|fletcher2|fletcher4|sha256|noparity|sha512|skein|edonr
    compression=on|off|gzip|gzip-N|lz4|lzjb|zle|zstd|zstd-N|zstd-fast|zstd-fast-N
    context=none|SELinux_User:SElinux_Role:Selinux_Type:Sensitivity_Level
    fscontext=none|SELinux_User:SElinux_Role:Selinux_Type:Sensitivity_Level
    defcontext=none|SELinux_User:SElinux_Role:Selinux_Type:Sensitivity_Level
    rootcontext=none|SELinux_User:SElinux_Role:Selinux_Type:Sensitivity_Level
    copies=1|2|3
    devices=on|off
    dedup=off|on|verify|sha256[,verify]|sha512[,verify]|skein[,verify]|edonr,verify
    dnodesize=legacy|auto|1k|2k|4k|8k|16k
    encryption=off|on|aes-128-ccm|aes-192-ccm|aes-256-ccm|aes-128-gcm|aes-192-gcm|aes-256-gcm
    keyformat=raw|hex|passphrase
    keylocation=prompt|file://</absolute/file/path>
    pbkdf2iters=iterations
    exec=on|off
    filesystem_limit=count|none
    special_small_blocks=size
    mountpoint=path|none|legacy
    nbmand=on|off
    overlay=on|off
    primarycache=all|none|metadata
    quota=size|none
    snapshot_limit=count|none
    recordsize=size
    readonly=on|off
    redundant_metadata=all|most
    refquota=size|none
    refreservation=size|none|auto
    relatime=on|off
    reservation=size|none
    secondarycache=all|none|metadata
    setuid=on|off
    sharesmb=on|off|opts
    sharenfs=on|off|opts
    logbias=latency|throughput
    snapdev=hidden|visible
    snapdir=hidden|visible
    sync=standard|always|disabled
    version=N|current
    volsize=size
    volmode=default | full | geom | dev | none
    vscan=on|off
    xattr=on|off|sa
    jailed=off | on
    casesensitivity=sensitive|insensitive|mixed
    normalization=none|formC|formD|formKC|formKD
    utf8only=on|off
"
    # Convert the list above into an array of strings
    set all_options (string split \n -- $all_options | string trim)

    # Make sure the name doesn't already have a trailing = (depending on the context)
    set name (string match -r '^[^=]+' -- $name)
    # Extract the values "value1|value2" from the long list of "key=value1|value2"
    set options (string match "$name=*" -- $all_options | string split =)[2]
    # echo o1: (string escape -- $options)
    # Put each possible value on its own line and account for formatting disparities
    set options (string replace -a '|' \n -- $options | string trim)
    # echo o2: (string escape -- $options)
    # Remove open-ended options (size, count, path)
    set options (string match -rv '^(size|count|iterations|path)$|\<.*?\>' -- $options)
    # echo o3: (string escape -- $options)
    # Expand optionally comma-separated arguments (e.g. skein[,verify] => skein\nskein,verify)
    set options (string replace -r "(.*)\[,(.*?)\]" '$1\n$1,$2' -- $options)
    # echo o4: (string escape -- $options)

    if set -q options[1]
        printf "$name=%s\n" $options
    end
end

# Generate a list of possible values from the man page.
# NB: This includes "hints" like "size" or "opts" which may be useful or unwanted. *shrug*
if type -q man && type -q col
    function __fish_zfs_property_options
        set -l property (string escape --style=regex -- $argv[1])
        set -l values (command man zfs | col -b | string replace -rf -- '^\s*'$property'=(.*\|.*)' '$1' | string split ' | ')
        printf '%s\n' $property=$values
    end
else
    function __fish_zfs_property_options
    end
end

function __fish_zfs_list_permissions -V OS
    echo -e "allow\tAlso needs the permission to be allowed"
    echo -e "clone\tAlso needs the 'create' and 'mount' permissions in the origin filesystem"
    echo -e "create\tAlso needs the 'mount' permission"
    echo -e "destroy\tAlso needs the 'mount' permission"
    echo -e mount
    echo -e "promote\tAlso needs the 'promote' and 'mount' permissions in the origin filesystem"
    echo -e "receive\tAlso needs the 'create' and 'mount' permissions"
    echo -e "rename\tAlso needs the 'create' and 'mount' permissions in the new parent"
    echo -e "rollback\tAlso needs the 'mount' permission"
    echo -e send
    echo -e "share\tFor SMB and NFS shares"
    echo -e "snapshot\tAlso needs the 'mount' permission"
    echo -e groupquota
    echo -e groupused
    echo -e "userprop\tAllows changing any user property"
    echo -e userquota
    echo -e userused
    # The remaining code of the function is almost a duplicate of __fish_complete_zfs_rw_properties and __fish_complete_zfs_ro_properties, but almost only, hence the duplication
    # RO properties
    echo -e "volblocksize\tVolume block size"
    # R/W properties
    echo -e "aclinherit\tInheritance of ACL entries (discard, noallow, restricted, passthrough, passthrough-x)"
    echo -e "atime\tUpdate access time on read (on, off)"
    echo -e "canmount\tIs the dataset mountable (on, off, noauto)"
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
    echo -e "checksum\tData checksum (on, off, fletcher2, fletcher4, sha256$additional_cksum_algs)"
    set -e additional_cksum_algs
    set -l additional_compress_algs ''
    if __fish_is_zfs_feature_enabled "feature@lz4_compress"
        set additional_compress_algs ", lz4"
    end
    echo -e "compression\tCompression algorithm (on, off, lzjb$additional_compress_algs, gzip, gzip-[1-9], zle)"
    set -e additional_compress_algs
    echo -e "copies\tNumber of copies of data (1, 2, 3)"
    echo -e "dedup\tDeduplication (on, off, verify, sha256[,verify]$additional_dedup_algs)"
    set -e additional_dedup_algs
    echo -e "devices\tAre contained device nodes openable (on, off)"
    echo -e "exec\tCan contained executables be executed (on, off)"
    echo -e "filesystem_limit\tMax number of filesystems and volumes (COUNT, none)"
    echo -e "mountpoint\tMountpoint (PATH, none, legacy)"
    echo -e "primarycache\tWhich data to cache in ARC (all, none, metadata)"
    echo -e "quota\tMax size of dataset and children (SIZE, none)"
    echo -e "snapshot_limit\tMax number of snapshots (COUNT, none)"
    echo -e "readonly\tRead-only (on, off)"
    echo -e "recordsize\tSuggest block size (SIZE)"
    echo -e "redundant_metadata\tHow redundant are the metadata (all, most)"
    echo -e "refquota\tMax space used by dataset itself (SIZE, none)"
    echo -e "refreservation\tMin space guaranteed to dataset itself (SIZE, none)"
    echo -e "reservation\tMin space guaranteed to dataset (SIZE, none)"
    echo -e "secondarycache\tWhich data to cache in L2ARC (all, none, metadata)"
    echo -e "setuid\tRespect set-UID bit (on, off)"
    echo -e "sharenfs\tShare in NFS (on, off, OPTS)"
    echo -e "logbias\tHint for handling of synchronous requests (latency, throughput)"
    echo -e "snapdir\tHide .zfs directory (hidden, visible)"
    echo -e "sync\tHandle of synchronous requests (standard, always, disabled)"
    echo -e "volsize\tVolume logical size (SIZE)"
    if test $OS = SunOS
        echo -e "zoned\tManaged from a non-global zone (on, off)"
        echo -e "mlslabel\tCan the dataset be mounted in a zone with Trusted Extensions enabled (LABEL, none)"
        echo -e "nbmand\tMount with Non Blocking mandatory locks (on, off)"
        echo -e "sharesmb\tShare in Samba (on, off)"
        echo -e "shareiscsi\tShare as an iSCSI target (on, off)"
        echo -e "version\tOn-disk version of filesystem (1, 2, current)"
        echo -e "vscan\tScan regular files for viruses on opening and closing (on, off)"
        echo -e "xattr\tExtended attributes (on, off, sa)"
    else if test $OS = Linux
        echo -e "acltype\tUse no ACL or POSIX ACL (noacl, posixacl)"
        echo -e "nbmand\tMount with Non Blocking mandatory locks (on, off)"
        echo -e "relatime\tSometimes update access time on read (on, off)"
        echo -e "shareiscsi\tShare as an iSCSI target (on, off)"
        echo -e "sharesmb\tShare in Samba (on, off)"
        echo -e "snapdev\tHide volume snapshots (hidden, visible)"
        echo -e "version\tOn-disk version of filesystem (1, 2, current)"
        echo -e "vscan\tScan regular files for viruses on opening and closing (on, off)"
        echo -e "xattr\tExtended attributes (on, off, sa)"
    else if test $OS = FreeBSD
        echo -e "aclmode\tHow is ACL modified by chmod (discard, groupmask, passthrough, restricted)"
        echo -e "volmode\tHow to expose volumes to OS (default, geom, dev, none)"
    end
    # Write-once properties
    echo -e "normalization\tUnicode normalization (none, formC, formD, formKC, formKD)"
    echo -e "utf8only\tReject non-UTF-8-compliant filenames (on, off)"
    if test $OS = Linux
        echo -e "casesensitivity\tCase sensitivity (sensitive, insensitive)"
    else # FreeBSD, SunOS and virtually all others
        echo -e "casesensitivity\tCase sensitivity (sensitive, insensitive, mixed)"
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

complete -c zfs -f -n __fish_zfs_needs_command -s '?' -a '?' -d 'Display a help message'
complete -c zfs -f -n __fish_zfs_needs_command -a create -d 'Create a volume or filesystem'
complete -c zfs -f -n __fish_zfs_needs_command -a destroy -d 'Destroy a dataset'
complete -c zfs -f -n __fish_zfs_needs_command -a 'snapshot snap' -d 'Create a snapshot'
complete -c zfs -f -n __fish_zfs_needs_command -a rollback -d 'Roll back a filesystem to a previous snapshot'
complete -c zfs -f -n __fish_zfs_needs_command -a clone -d 'Create a clone of a snapshot'
complete -c zfs -f -n __fish_zfs_needs_command -a promote -d 'Promotes a clone file system to no longer be dependent on its "origin" snapshot'
complete -c zfs -f -n __fish_zfs_needs_command -a rename -d 'Rename a dataset'
complete -c zfs -f -n __fish_zfs_needs_command -a list -d 'List dataset properties'
complete -c zfs -f -n __fish_zfs_needs_command -a set -d 'Set a dataset property'
complete -c zfs -f -n __fish_zfs_needs_command -a get -d 'Get one or several dataset properties'
complete -c zfs -f -n __fish_zfs_needs_command -a inherit -d 'Set a dataset property to be inherited'
complete -c zfs -f -n __fish_zfs_needs_command -a upgrade -d 'List upgradeable datasets, or upgrade one'
complete -c zfs -f -n __fish_zfs_needs_command -a userspace -d 'Get dataset space consumed by each user'
complete -c zfs -f -n __fish_zfs_needs_command -a groupspace -d 'Get dataset space consumed by each user group'
complete -c zfs -f -n __fish_zfs_needs_command -a mount -d 'Mount a filesystem'
complete -c zfs -f -n __fish_zfs_needs_command -a 'unmount umount' -d 'Unmount a filesystem'
complete -c zfs -f -n __fish_zfs_needs_command -a share -d 'Share a given filesystem, or all of them'
complete -c zfs -f -n __fish_zfs_needs_command -a unshare -d 'Stop sharing a given filesystem, or all of them'
if __fish_is_zfs_feature_enabled 'feature@bookmarks'
    complete -c zfs -f -n __fish_zfs_needs_command -a bookmark -d 'Create a bookmark for a snapshot'
end
complete -c zfs -f -n __fish_zfs_needs_command -a send -d 'Output a stream representation of a dataset'
complete -c zfs -f -n __fish_zfs_needs_command -a 'receive recv' -d 'Write on disk a dataset from the stream representation given on standard input'
complete -c zfs -f -n __fish_zfs_needs_command -a allow -d 'Delegate, or display delegations of, rights on a dataset to (groups of) users'
complete -c zfs -f -n __fish_zfs_needs_command -a unallow -d 'Revoke delegations of rights on a dataset from (groups of) users'
complete -c zfs -f -n __fish_zfs_needs_command -a hold -d 'Put a named hold on a snapshot'
complete -c zfs -f -n __fish_zfs_needs_command -a holds -d 'List holds on a snapshot'
complete -c zfs -f -n __fish_zfs_needs_command -a release -d 'Remove a named hold from a snapshot'
complete -c zfs -f -n __fish_zfs_needs_command -a diff -d 'List changed files between a snapshot, and a filesystem or a previous snapshot'
if test $OS = SunOS # This is currently only supported under Illumos, but that will probably change
    complete -c zfs -f -n __fish_zfs_needs_command -a program -d 'Execute a ZFS Channel Program'
end

# Completions hereafter try to follow the man pages commands order, for maintainability, at the cost
# of multiple if statements.

# create completions
complete -c zfs -f -n '__fish_zfs_using_command create' -s p -d 'Create all needed non-existing parent datasets'
if test $OS = Linux # Only Linux supports the comma-separated format; others need multiple -o calls
    complete -c zfs -x -n '__fish_zfs_using_command create' -s o -d 'Dataset property' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties))'
end
complete -c zfs -x -n '__fish_zfs_using_command create' -s o -d 'Dataset property' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_zfs_complete_property_values (commandline -t))'
# create completions for volumes; as -V is necessary to zfs to recognize a volume creation request,
# we use it as a condition to propose volume creation completions.
# If -V is typed after -s or -b, zfs should accept it, but fish won't propose -s or -b, but, as
# these options are for volumes only, it seems reasonable to expect the user to ask for a volume,
# with -V, before giving its characteristics with -s or -b
complete -c zfs -x -n '__fish_zfs_using_command create' -s V -d 'Volume size'
complete -c zfs -f -n '__fish_zfs_using_command create; and __fish_contains_opt -s V' -s s -d 'Create a sparse volume'
complete -c zfs -x -n '__fish_zfs_using_command create; and __fish_contains_opt -s V' -s b -d Blocksize
# new dataset completions, applicable for both regular datasets and volumes; must start with pool
# and may optionally be a child of a pre-existing dataset.
complete -c zfs -x -n '__fish_zfs_using_command create' -a '(printf "%s/\n" (__fish_print_zfs_filesystems))'

# destroy completions; as the dataset is the last item, we can't know yet if it's a snapshot, a
# bookmark or something else, so we can't separate snapshot-specific options from others.
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
if test $OS = Linux # Only Linux supports the comma-separated format; others need multiple -o calls
    complete -c zfs -x -n '__fish_zfs_using_command snapshot; or __fish_zfs_using_command snap' -s o -d 'Snapshot property' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties))'
end
complete -c zfs -x -n '__fish_zfs_using_command snapshot; or __fish_zfs_using_command snap' -s o -d 'Snapshot property' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_zfs_complete_property_values (commandline -t))'
complete -c zfs -x -n '__fish_zfs_using_command snapshot; or __fish_zfs_using_command snap' -d 'Dataset to snapshot' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes)'

# rollback completions
complete -c zfs -f -n '__fish_zfs_using_command rollback' -s r -d 'Destroy later snapshot and bookmarks'
complete -c zfs -f -n '__fish_zfs_using_command rollback' -s R -d 'Recursively destroy later snapshot, bookmarks and clones'
complete -c zfs -f -n '__fish_zfs_using_command rollback; and __fish_contains_opt -s R' -s f -d 'Force unmounting of clones'
complete -c zfs -x -n '__fish_zfs_using_command rollback' -d 'Snapshot to roll back to' -a '(__fish_print_zfs_snapshots --force)'

# clone completions
complete -c zfs -f -n '__fish_zfs_using_command clone' -s p -d 'Create all needed non-existing parent datasets'
if test $OS = Linux # Only Linux supports the comma-separated format; others need multiple -o calls
    complete -c zfs -x -n '__fish_zfs_using_command clone' -s o -d 'Clone property' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties))'
end
complete -c zfs -x -n '__fish_zfs_using_command clone' -s o -d 'Clone property' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_zfs_complete_property_values (commandline -t))'
complete -c zfs -x -n '__fish_zfs_using_command clone' -d 'Snapshot to clone' -a '(__fish_print_zfs_snapshots --force)'

# promote completions
complete -c zfs -x -n '__fish_zfs_using_command promote' -d 'Clone to promote' -a '(__fish_print_zfs_filesystems)'

# rename completions; as the dataset is the last item, we can't know yet if it's a snapshot or not,
# we can't separate snapshot-specific option from others.
complete -c zfs -f -n '__fish_zfs_using_command rename' -s p -d 'Create all needed non-existing parent datasets'
complete -c zfs -f -n '__fish_zfs_using_command rename' -s r -d 'Recursively rename children snapshots'
if __fish_is_openzfs
    complete -c zfs -f -n '__fish_zfs_using_command rename' -s f -d 'Force unmounting if needed'
end
# These FreeBSD completions are in addition to any added via the OpenZFS check above
if test $OS = FreeBSD
    complete -c zfs -f -n '__fish_zfs_using_command rename' -s u -d 'Do not remount filesystems during rename'
end
complete -c zfs -x -n '__fish_zfs_using_command rename' -d 'Dataset to rename' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# list completions
complete -c zfs -f -n '__fish_zfs_using_command list' -s H -d 'Print output in a machine-parsable format'
complete -c zfs -f -n '__fish_zfs_using_command list' -s r -d 'Operate recursively on datasets'
complete -c zfs -x -n '__fish_zfs_using_command list; and __fish_contains_opt -s r' -s d -d 'Maximum recursion depth'
complete -c zfs -x -n '__fish_zfs_using_command list' -s o -d 'Property to list' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo -e "name\tDataset name"; echo -e "space\tSpace properties"))'
complete -c zfs -f -n '__fish_zfs_using_command list' -s p -d 'Print parsable (exact) values for numbers'
complete -c zfs -x -n '__fish_zfs_using_command list; and __fish_not_contain_opt -s S' -s s -d 'Property to use for sorting output by ascending order' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo -e "name\tDataset name")'
complete -c zfs -x -n '__fish_zfs_using_command list; and __fish_not_contain_opt -s s' -s S -d 'Property to use for sorting output by descending order' -a '(__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo -e "name\tDataset name")'
complete -c zfs -x -n '__fish_zfs_using_command list' -s t -d 'Dataset type' -a '(__fish_zfs_list_dataset_types)'
complete -c zfs -x -n '__fish_zfs_using_command list' -d 'Dataset whose properties to list' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# set completions (zfs set PROP VALUE POOL/DATASET)
complete -c zfs -x -n '__fish_zfs_using_command set; and __fish_is_nth_token 2' -d 'Property to set' -a '(__fish_complete_zfs_rw_properties)'
complete -c zfs -x -n '__fish_zfs_using_command set; and __fish_is_nth_token 3' -d 'Dataset whose property to set' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'
# set property value completions
complete -c zfs -x -n '__fish_zfs_using_command set; and __fish_is_nth_token 2' -a '(__fish_zfs_complete_property_values (__fish_nth_token 2))'

# get completions
complete -c zfs -f -n '__fish_zfs_using_command get' -s r -d 'Operate recursively on datasets'
complete -c zfs -x -n '__fish_zfs_using_command get; and __fish_contains_opt -s r' -s d -d 'Maximum recursion depth'
complete -c zfs -f -n '__fish_zfs_using_command get' -s H -d 'Print output in a machine-parsable format'
complete -c zfs -x -n '__fish_zfs_using_command get' -s o -d 'Fields to display' -a '(__fish_append , (__fish_zfs_list_get_fields))'
complete -c zfs -x -n '__fish_zfs_using_command get' -s s -d 'Property source to display' -a '(__fish_append , (__fish_zfs_list_source_types))'
complete -c zfs -f -n '__fish_zfs_using_command get' -s p -d 'Print parsable (exact) values for numbers'
complete -c zfs -x -n '__fish_zfs_using_command get' -s t -d 'Dataset type' -a '(__fish_append , (__fish_zfs_list_dataset_types))'
complete -c zfs -x -n '__fish_zfs_using_command get' -d 'Property to get' -a '(__fish_append , (__fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties; __fish_complete_zfs_ro_properties; echo "all"))'
complete -c zfs -x -n '__fish_zfs_using_command get' -d 'Dataset whose property to get' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

# inherit completions
complete -c zfs -f -n '__fish_zfs_using_command inherit' -s r -d 'Operate recursively on datasets'
complete -c zfs -f -n '__fish_zfs_using_command inherit' -s S -d 'Revert to the received value if available'
complete -c zfs -x -n '__fish_zfs_using_command inherit' -d 'Dataset whose properties are to be inherited' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes; __fish_print_zfs_snapshots)'

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
complete -c zfs -x -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace; and __fish_not_contain_opt -s S' -s s -d 'Property to use for sorting output by ascending order' -a __fish_zfs_list_space_fields
complete -c zfs -x -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace; and __fish_not_contain_opt -s s' -s S -d 'Property to use for sorting output by descending order' -a __fish_zfs_list_space_fields
complete -c zfs -x -n '__fish_zfs_using_command userspace' -s t -d 'Identity types to display' -a '(__fish_append , (__fish_zfs_list_userspace_types))'
complete -c zfs -x -n '__fish_zfs_using_command groupspace' -s t -d 'Identity types to display' -a '(__fish_append , (__fish_zfs_list_groupspace_types))'
complete -c zfs -f -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace' -s i -d 'Translate S(amba)ID to POSIX ID'
complete -c zfs -x -n '__fish_zfs_using_command userspace; or __fish_zfs_using_command groupspace' -d 'Dataset whose space usage to get' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_snapshots)'

# mount completions
if test $OS = Linux
    complete -c zfs -x -n '__fish_zfs_using_command mount' -s o -d 'Temporary mount point property' -a '(__fish_append , (__fish_complete_zfs_mountpoint_properties))'
end
complete -c zfs -x -n '__fish_zfs_using_command mount' -s o -d 'Temporary mount point property' -a '(__fish_complete_zfs_mountpoint_properties; __fish_zfs_complete_property_values (commandline -t))'
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
complete -c zfs -x -n '__fish_zfs_using_command send' -s i -d 'Generate incremental stream from snapshot' -a '(__fish_print_zfs_snapshots --force; __fish_print_zfs_bookmarks)'
complete -c zfs -x -n '__fish_zfs_using_command send' -s I -d 'Generate incremental stream from snapshot, even with intermediary snapshots' -a '(__fish_print_zfs_snapshots --force)'
if test $OS = SunOS
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
# Only snapshots (and bookmarks?) may be sent
complete -c zfs -x -n '__fish_zfs_using_command send' -d 'Dataset to send' -a '(__fish_print_zfs_snapshots --force)'

# receive completions
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s v -d 'Print verbose information about the stream and the time spent processing it'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s n -d 'Dry run: do not actually receive the stream'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s F -d 'Force rollback to the most recent snapshot'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s u -d 'Unmount the target filesystem'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv; and __fish_not_contain_opt -s e' -s d -d 'Discard the first element of the path of the sent snapshot'
complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv; and __fish_not_contain_opt -s d' -s e -d 'Discard all but the last element of the path of the sent snapshot'
if test $OS = SunOS
    complete -c zfs -x -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s o -d 'Force the stream to be received as a clone of the given snapshot' -a origin
end
if __fish_is_zfs_feature_enabled "feature@extensible_dataset"
    complete -c zfs -f -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -s s -d 'On transfer interruption, store a receive_resume_token for resumption'
end
complete -c zfs -x -n '__fish_zfs_using_command receive; or __fish_zfs_using_command recv' -d 'Dataset to receive' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes;)'

# allow completions
complete -c zfs -f -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s d' -s l -d 'Delegate permissions only on the specified dataset'
complete -c zfs -f -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s l' -s d -d 'Delegate permissions only on the descendents dataset'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s e' -s u -d 'User to delegate permissions to' -a '(__fish_append , (__fish_complete_users))'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s e' -s g -d 'Group to delegate permissions to' -a '(__fish_append , (__fish_complete_groups))'
if contains -- $OS SunOS FreeBSD
    complete -c zfs -f -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s u -s g -s e' -a everyone -d 'Delegate permission to everyone'
end
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s u -s g everyone' -s e -d 'Delegate permission to everyone'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s s' -s c -d 'Delegate permissions only to the creator of later descendent datasets' -a '(__fish_append , (__fish_zfs_list_permissions))'
complete -c zfs -x -n '__fish_zfs_using_command allow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s c' -s s -d 'Create a permission set or add permissions to an existing one'
complete -c zfs -x -n '__fish_zfs_using_command allow' -d 'Dataset on which delegation is to be applied' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes)'

# unallow completions
complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s d' -s l -d 'Remove permissions only on the specified dataset'
complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s l' -s d -d 'Remove permissions only on the descendents dataset'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s e' -s u -d 'User to remove permissions from' -a '(__fish_append , (__fish_complete_users))'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s e' -s g -d 'Group to remove permissions from' -a '(__fish_append , (__fish_complete_groups))'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s u -s g everyone' -s e -d 'Remove permission from everyone'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s s' -s c -d 'Remove permissions only on later created descendent datasets' -a '(__fish_append , (__fish_zfs_list_permissions))'
complete -c zfs -x -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s l -s d -s e -s g -s u -s c' -s s -d 'Remove a permission set or remove permissions from an existing one'
if test $OS = SunOS
    complete -c zfs -f -n '__fish_zfs_using_command unallow' -s r -d 'Remove permissions recursively'
    complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s u -s g -s e' -a everyone -d 'Remove permission from everyone'
else if test $OS = FreeBSD
    complete -c zfs -f -n '__fish_zfs_using_command unallow; and __fish_not_contain_opt -s u -s g -s e' -a everyone -d 'Remove permission from everyone'
end
complete -c zfs -x -n '__fish_zfs_using_command unallow' -d 'Dataset on which delegation is to be removed' -a '(__fish_print_zfs_filesystems; __fish_print_zfs_volumes)'

# hold completions
complete -c zfs -f -n '__fish_zfs_using_command hold' -s r -d 'Apply hold recursively'
complete -c zfs -x -n '__fish_zfs_using_command hold' -d 'Snapshot on which hold is to be applied' -a '(__fish_print_zfs_snapshots)'

# holds completions
complete -c zfs -f -n '__fish_zfs_using_command holds' -s r -d 'List holds recursively'
complete -c zfs -x -n '__fish_zfs_using_command holds' -d 'Snapshot whose holds are to be listed' -a '(__fish_print_zfs_snapshots)'

# release completions
complete -c zfs -f -n '__fish_zfs_using_command release' -s r -d 'Release hold recursively'
complete -c zfs -x -n '__fish_zfs_using_command release' -d 'Snapshot from which hold is to be removed' -a '(__fish_print_zfs_snapshots)'

# diff completions
complete -c zfs -f -n '__fish_zfs_using_command diff' -s F -d 'Display file type (à la ls -F)'
complete -c zfs -f -n '__fish_zfs_using_command diff' -s H -d 'Print output in a machine-parsable format'
complete -c zfs -f -n '__fish_zfs_using_command diff' -s t -d 'Display inode change time'
complete -c zfs -x -n '__fish_zfs_using_command diff' -d 'Source snapshot for the diff' -a '(__fish_print_zfs_snapshots)'

# program completions
if test $OS = SunOS # This is currently only supported under Illumos, but that will probably change
    complete -c zfs -x -n '__fish_zfs_using_command program' -s t -d 'Execution timeout'
    complete -c zfs -x -n '__fish_zfs_using_command program' -s t -d 'Execution memory limit'
    complete -c zfs -x -n '__fish_zfs_using_command program' -d 'Pool program will be executed on' -a '(__fish_complete_zfs_pools)'
end
