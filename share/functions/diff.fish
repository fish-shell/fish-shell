# Use colours in diff output, if supported
if command -vq diff; and command diff --color=auto /dev/null{,} >/dev/null 2>&1
    function diff
        command diff --color=auto $argv
    end
end
