function __fish_print_zfs_filesystems -d "Lists ZFS filesystems"
	zfs list -t filesystem -o name -H
end
