function export --description 'Set global variable. Alias for set -gx, made for bash compatibility'
    if test -z "$argv"
        set
        return 0
    end
    for arg in $argv
        set -l v (string split -m 1 "=" -- $arg)
        switch (count $v)
            case 1
                set -gx $v $$v
            case 2
                set -gx $v[1] $v[2]
        end
    end
end
