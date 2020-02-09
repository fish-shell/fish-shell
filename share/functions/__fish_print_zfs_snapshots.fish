function __fish_print_zfs_snapshots -d "Lists ZFS snapshots"
    zfs list -t snapshot -o name -H
end
