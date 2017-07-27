# Show information about the named var(s) passed to us.
function show --no-scope-shadowing
    for _v in $argv
        if set -q $_v
            set -l _c (count $$_v)
            printf '$%s count=%d\n' $_v $_c
            # This test is to work around a bogosity of the BSD `seq` command. If called with
            # `seq 0` it emits `1`, `0`. Whereas that GNU version outputs nothing.
            if test $_c -gt 0
                for i in (seq $_c)
                    printf '$%s[%d]=|%s|\n' $_v $i $$_v[1][$i]
                end
            end
        else
            echo \$$_v is not set
        end
    end
end
