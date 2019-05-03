function __fish_complete_zfs_rw_properties -d "Completes with ZFS read-write properties"
    set -l OS ""
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
    echo -e "aclinherit\t"(_ "Inheritance of ACL entries")" (discard, noallow, restricted, passthrough, passthrough-x)"
    echo -e "atime\t"(_ "Update access time on read")" (on, off)"
    echo -e "canmount\t"(_ "Is the dataset mountable")" (on, off, noauto)"
    set -l additional_algs ''
    if contains -- $OS FreeBSD SunOS
        set additional_algs "$additional_algs, noparity"
    end
    if __fish_is_zfs_feature_enabled "feature@sha512"
        set additional_algs "$additional_algs, sha512"
    end
    if __fish_is_zfs_feature_enabled "feature@skein"
        set additional_algs "$additional_algs, skein"
    end
    if __fish_is_zfs_feature_enabled "feature@edonr"
        set additional_algs "$additional_algs, edonr"
    end
    echo -e "checksum\t"(_ "Data checksum")" (on, off, fletcher2, fletcher4, sha256$additional_algs)"
    if __fish_is_zfs_feature_enabled "feature@lz4_compress"
        set additional_algs ", lz4"
    end
    echo -e "compression\t"(_ "Compression algorithm")" (on, off, lzjb$additional_algs, gzip, gzip-[1-9], zle)"
    set -e additional_algs
    echo -e "copies\t"(_ "Number of copies of data")" (1, 2, 3)"
    echo -e "dedup\t"(_ "Deduplication")" (on, off, verify, sha256[,verify])"
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

    # Autogenerate userquota@$USER list; only usernames are supported by the completion, but the zfs command supports more formats
    for user in (__fish_print_users)
        set -l tabAndBefore (echo -e "userquota@$user\t")
        printf (_ "%sMax usage by user %s\n") $tabAndBefore $user
    end
    # Autogenerate groupquota@$USER list
    for group in (__fish_print_groups)
        set -l tabAndBefore (echo -e "groupquota@$group\t")
        printf (_ "%sMax usage by group %s\n") $tabAndBefore $group
    end
    if test $OS = "SunOS"
        echo -e "aclmode\t"(_ "How is ACL modified by chmod")" (discard, groupmask, passthrough, restricted)"
        echo -e "mlslabel\t"(_ "Can the dataset be mounted in a zone with Trusted Extensions enabled")" (LABEL, none)"
        echo -e "nbmand\t"(_ "Mount with Non Blocking mandatory locks")" (on, off)"
        echo -e "sharesmb\t"(_ "Share in Samba")" (on, off)"
        echo -e "shareiscsi\t"(_ "Share as an iSCSI target")" (on, off)"
        echo -e "version\t"(_ "On-disk version of filesystem")" (1, 2, current)"
        echo -e "vscan\t"(_ "Scan regular files for viruses on opening and closing")" (on, off)"
        echo -e "xattr\t"(_ "Extended attributes")" (on, off, sa)"
        echo -e "zoned\t"(_ "Managed from a non-global zone")" (on, off)"
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
    # User properties; the /dev/null redirection masks the possible "no datasets available"
    zfs list -o all 2>/dev/null | head -n 1 | string replace -a -r "\s+" "\n" | string match -e ":" | string lower
end
