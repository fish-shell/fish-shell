function __fish_bind_filter --description 'Enable only legacy / only new or both bindings based on $FISH_BIND_FILTER'
    set filter (string split -- ' ' $FISH_BIND_FILTER)
    if not set -q filter[1]
        set filter v2 # only new
    end
    if test $argv[1] = $filter[1]
        $argv[2..]
    end
end
