#
# Match colors for grep, if supported
#

if command grep --color=auto --help 1>/dev/null 2>/dev/null
	function grep
		command grep --color=auto $argv
	end
end

