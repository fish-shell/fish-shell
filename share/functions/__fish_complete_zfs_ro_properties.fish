function __fish_complete_zfs_ro_properties -d "Completes with ZFS read-only properties"
    echo -e "available\tAvailable space"
    echo -e "avail\tAvailable space"
    echo -e "compressratio\tAchieved compression ratio"
    echo -e "creation\tDataset creation time"
    echo -e "clones\tSnapshot clones"
    echo -e "defer_destroy\tMarked for deferred destruction"
    echo -e "filesystem_count\tTotal lower-level filesystems and volumes"
    echo -e "logicalreferenced\tLogical total space"
    echo -e "lrefer\tLogical total space"
    echo -e "logicalused\tLogical used space"
    echo -e "lused\tLogical used space"
    echo -e "mounted\tIs currently mounted?"
    echo -e "origin\tSource snapshot"
    if __fish_is_zfs_feature_enabled "feature@extensible_dataset"
        echo -e "receive_resume_token\tToken for interrupted reception resumption"
    end
    echo -e "referenced\tTotal space"
    echo -e "refer\tTotal space"
    echo -e "refcompressratio\tAchieved compression ratio of referenced space"
    echo -e "snapshot_count\tTotal lower-level snapshots"
    echo -e "type\tDataset type"
    echo -e "used\tSpace used by dataset and children"
    echo -e "usedbychildren\tSpace used by children datasets"
    echo -e "usedbydataset\tSpace used by dataset itself"
    echo -e "usedbyrefreservation\tSpace used by refreservation"
    echo -e "usedbysnapshots\tSpace used by dataset snapshots"
    echo -e "userrefs\tHolds count"
    echo -e "volblocksize\tVolume block size"
    echo -e "written\tReferenced data written since previous snapshot"
    # Autogenerate userused@$USER list; only usernames are supported by the completion, but the zfs command supports more formats
    for user in (__fish_print_users)
        set -l tabAndBefore (echo -e "userused@$user\t")
        printf "%sDataset space use by user %s\n" $tabAndBefore $user
    end
    # Autogenerate groupused@$USER list
    for group in (__fish_print_groups)
        set -l tabAndBefore (echo -e "groupused@$group\t")
        printf "%sDataset space use by group %s\n" $tabAndBefore $group
    end
    # Autogenerate written@$SNAPSHOT list
    for snapshot in (__fish_print_zfs_snapshots)
        set -l tabAndBefore (echo -e "written@$snapshot\t")
        printf "%sReferenced data written since snapshot %s\n" $tabAndBefore $snapshot
    end
end
