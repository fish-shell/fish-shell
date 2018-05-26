function __fish_is_zfs_feature_enabled -a feature target -d "Returns 0 if the given ZFS feature is available or enabled for the given full-path target (zpool or dataset), or any target if none given"
    set -l pool (string replace -r '/.*' '' -- $target)
    set -l feature_name ""
    if test -z "$pool"
        set feature_name (zpool get -H all | string match -r "\s$feature\s")
    else
        set feature_name (zpool get -H all $pool | string match -r "$pool\s$feature\s")
    end
    if test $status -ne 0 # No such feature
        return 1
    end
    echo $feature_name | read -l _ _ state _
    set -l state (echo $feature_name | cut -f3)
    string match -qr '(active|enabled)' -- $state
    return $status
end
