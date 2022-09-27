function __fish_seen_argument_from
    for arg in $argv
        if __fish_seen_argument -- $arg
            return 0
        end
    end
    return 1
end
