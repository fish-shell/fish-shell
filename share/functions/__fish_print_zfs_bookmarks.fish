function __fish_print_zfs_bookmarks -d "Lists ZFS bookmarks, if the feature is enabled"
	if __fish_is_zfs_feature_enabled 'feature@bookmarks'
		zfs list -t bookmark -o name -H
	end
end
