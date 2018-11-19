#
# Enable ip colored output if this is available
# But only when writing to stdout
#

if command -sq ip; and command ip -c l >/dev/null 2>/dev/null
    function ip
        if isatty stdout
            command ip -c $argv
        else
            command ip $argv
        end
    end
end
