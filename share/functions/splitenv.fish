# splitenv inputs a list of variable names, splits them about colons, and then sets them back again.
function splitenv --no-scope-shadowing
    for arg in $argv
        set -q $arg; and set $arg (string split ":" $$arg)
    end
end
