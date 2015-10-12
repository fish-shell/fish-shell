function export --description 'Set global variable. Alias for set -gx, made for bash compatibility'
    if test -z "$argv"
        set
        return 0
    end
    for arg in $argv
        # Only split on the first =
        # The literal "\n" is necessary because string doesn't interpret it without -r
        set -l v (echo $arg | string replace "=" \n)
        set -l c (count $v)
        switch $c
            case 1
                set -gx $v $$v
            case 2
                set -gx $v[1] $v[2]
        end
    end
end
