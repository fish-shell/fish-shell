function __fish_print_zfs_volumes -d "Lists ZFS volumes"
	zfs list -t volume -o name -H
end
