function __fish_complete_zfs_ro_properties -d "Completes with ZFS read-only properties"
    echo -e "available\t"(_ "Available space")
    echo -e "avail\t"(_ "Available space")
    echo -e "compressratio\t"(_ "Achieved compression ratio")
    echo -e "creation\t"(_ "Dataset creation time")
    echo -e "clones\t"(_ "Snapshot clones")
    echo -e "defer_destroy\t"(_ "Marked for deferred destruction")
    echo -e "filesystem_count\t"(_ "Total lower-level filesystems and volumes")
    echo -e "logicalreferenced\t"(_ "Logical total space")
    echo -e "lrefer\t"(_ "Logical total space")
    echo -e "logicalused\t"(_ "Logical used space")
    echo -e "lused\t"(_ "Logical used space")
    echo -e "mounted\t"(_ "Is currently mounted?")
    echo -e "origin\t"(_ "Source snapshot")
    if __fish_is_zfs_feature_enabled "feature@extensible_dataset"
        echo -e "receive_resume_token\t"(_ "Token for interrupted reception resumption")
    end
    echo -e "referenced\t"(_ "Total space")
    echo -e "refer\t"(_ "Total space")
    echo -e "refcompressratio\t"(_ "Achieved compression ratio of referenced space")
    echo -e "snapshot_count\t"(_ "Total lower-level snapshots")
    echo -e "type\t"(_ "Dataset type")
    echo -e "used\t"(_ "Space used by dataset and children")
    echo -e "usedbychildren\t"(_ "Space used by children datasets")
    echo -e "usedbydataset\t"(_ "Space used by dataset itself")
    echo -e "usedbyrefreservation\t"(_ "Space used by refreservation")
    echo -e "usedbysnapshots\t"(_ "Space used by dataset snapshots")
    echo -e "userrefs\t"(_ "Holds count")
    echo -e "volblocksize\t"(_ "Volume block size")
    echo -e "written\t"(_ "Referenced data written since previous snapshot")
    # Autogenerate userused@$USER list; only usernames are supported by the completion, but the zfs command supports more formats
    for user in (__fish_print_users)
        set -l tabAndBefore (echo -e "userused@$user\t")
        printf (_ "%sDataset space use by user %s\n") $tabAndBefore $user
    end
    # Autogenerate groupused@$USER list
    for group in (__fish_print_groups)
        set -l tabAndBefore (echo -e "groupused@$group\t")
        printf (_ "%sDataset space use by group %s\n") $tabAndBefore $group
    end
    # Autogenerate written@$SNAPSHOT list
    for snapshot in (__fish_print_zfs_snapshots)
        set -l tabAndBefore (echo -e "written@$snapshot\t")
        printf (_ "%sReferenced data written since snapshot %s\n") $tabAndBefore $snapshot
    end
end
