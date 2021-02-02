#
# Enable ip colored output if available
#

if command -sq ip; and command ip -color=auto link >/dev/null 2>/dev/null
    function ip
        command ip -color=auto $argv
    end
end
