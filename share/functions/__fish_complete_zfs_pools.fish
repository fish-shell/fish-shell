function __fish_complete_zfs_pools -d "Completes with available ZFS pools"
	zpool list -o name,comment -H | sed -e 's/\t-//g'
end
