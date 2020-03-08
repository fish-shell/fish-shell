function __fish_complete_zfs_rw_properties -d "Completes with ZFS read-write properties"
    set -l OS ""
    switch (uname)
        case Linux
            set OS Linux
        case Darwin
            set OS macOS
        case FreeBSD
            set OS FreeBSD
        case SunOS
            set OS SunOS
            # Others?
        case "*"
            set OS unknown
    end
    echo -e "aclinherit\tInheritance of ACL entries (discard, noallow, restricted, passthrough, passthrough-x)"
    echo -e "atime\tUpdate access time on read (on, off)"
    echo -e "canmount\tIs the dataset mountable (on, off, noauto)"
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
    echo -e "checksum\tData checksum (on, off, fletcher2, fletcher4, sha256$additional_algs)"
    if __fish_is_zfs_feature_enabled "feature@lz4_compress"
        set additional_algs ", lz4"
    end
    echo -e "compression\tCompression algorithm (on, off, lzjb$additional_algs, gzip, gzip-[1-9], zle)"
    set -e additional_algs
    echo -e "copies\tNumber of copies of data (1, 2, 3)"
    echo -e "dedup\tDeduplication (on, off, verify, sha256[,verify])"
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

    # Autogenerate userquota@$USER list; only usernames are supported by the completion, but the zfs command supports more formats
    for user in (__fish_print_users)
        set -l tabAndBefore (echo -e "userquota@$user\t")
        printf "%sMax usage by user %s\n" $tabAndBefore $user
    end
    # Autogenerate groupquota@$USER list
    for group in (__fish_print_groups)
        set -l tabAndBefore (echo -e "groupquota@$group\t")
        printf "%sMax usage by group %s\n" $tabAndBefore $group
    end
    if test $OS = SunOS
        echo -e "aclmode\tHow is ACL modified by chmod (discard, groupmask, passthrough, restricted)"
        echo -e "mlslabel\tCan the dataset be mounted in a zone with Trusted Extensions enabled (LABEL, none)"
        echo -e "nbmand\tMount with Non Blocking mandatory locks (on, off)"
        echo -e "sharesmb\tShare in Samba (on, off)"
        echo -e "shareiscsi\tShare as an iSCSI target (on, off)"
        echo -e "version\tOn-disk version of filesystem (1, 2, current)"
        echo -e "vscan\tScan regular files for viruses on opening and closing (on, off)"
        echo -e "xattr\tExtended attributes (on, off, sa)"
        echo -e "zoned\tManaged from a non-global zone (on, off)"
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
    # User properties; the /dev/null redirection masks the possible "no datasets available"
    zfs list -o all 2>/dev/null | head -n 1 | string replace -a -r "\s+" "\n" | string match -e ":" | string lower
end
