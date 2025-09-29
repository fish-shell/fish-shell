# localization: tier1
function export --description 'Set env variable. Alias for `set -gx` for bash compatibility.'
    if not set -q argv[1]
        set -x
        return 0
    end
    for arg in $argv
        set -l v (string split -m 1 "=" -- $arg)
        set -l value
        switch (count $v)
            case 1
                set value $$v[1]
            case 2
                set value $v[2]
        end
        set -gx $v[1] $value
    end
end
