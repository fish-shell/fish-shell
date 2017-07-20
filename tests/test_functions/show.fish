# Show information about the named var(s) passed to us.
function show --no-scope-shadowing
    for v in $argv
        if set -q $v
            set -l c (count $$v)
            printf '$%s count=%d\n' $v $c
            # This test is to work around a bogosity of the BSD `seq` command. If called with
            # `seq 0` it emits `1`, `0`. Whereas that GNU version outputs nothing.
            if test $c -gt 0
                for i in (seq $c)
                    printf '$%s[%d]=|%s|\n' $v $i $$v[1][$i]
                end
            end
        else
            echo \$$v is not set
        end
    end
end
