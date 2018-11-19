#
# Enable ip colored output if this is available
#

if command ip -c l >/dev/null 2>/dev/null
    function ip
        command ip -c $argv
    end
end
