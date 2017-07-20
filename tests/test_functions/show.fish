# Show information about the named var(s) passed to us.
function show --no-scope-shadowing
    for v in $argv
        if set -q $v
            set -l c (count $$v)
            printf '$%s count=%d\n' $v $c
            for i in (seq $c)
                printf '$%s[%d]=|%s|\n' $v $i $$v[1][$i]
            end
        else
            echo \$$v is not set
        end
    end
end
