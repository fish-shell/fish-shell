function __fish_complete_zfs_pools -d "Completes with available ZFS pools"
    zpool list -o name,comment -H | string replace -a \t'-' ''
end
