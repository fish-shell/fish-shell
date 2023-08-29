function __fish_is_zfs_feature_enabled \
    -a feature pool \
    -d "Returns 0 if the given ZFS pool feature is active or enabled for the given pool or for any pool if none specified"

    type -q zpool || return
    zpool get -H -o value $feature $pool 2>/dev/null | string match -rq '^(enabled|active)$'
end
