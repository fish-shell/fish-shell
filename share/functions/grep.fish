#
# Match colors for grep, if supported
#

if echo | command grep --color=auto "" >/dev/null 2>&1
    function grep
        command grep --color=auto $argv
    end
end
