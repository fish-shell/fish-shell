function __fish_is_zfs_feature_enabled -a feature target -d "Returns 0 if the given ZFS feature is available or enabled for the given full-path target (zpool or dataset), or any target if none given"
	set -l pool (echo "$target" | cut -d '/' -f1)
    set -l feature_name ""
    set -l pattern "[[:space:]]"
    if test -z "$pool"
        set pattern "$pattern$feature$pattern"
        set feature_name (zpool get all -H | grep "$pattern")
    else
        set pattern "$pool$pattern$feature$pattern"
        set feature_name (zpool get all -H $pool | grep "$pattern")
    end
	if test $status -ne 0 # No such feature
		return 1
	end
	set -l state (echo $feature_name | cut -f3)
    echo "$feature_name" | grep -q -E "(active|enabled)"
    return $status
end
