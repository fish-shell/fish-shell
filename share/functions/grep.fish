#
# Match colors for grep, if supported
#

if command grep --color=auto --help 1>/dev/null 2>/dev/null
	if not set -q GREP_COLOR
		set -gx GREP_COLOR '97;45'
	end
	function grep
		command grep --color=auto $argv
	end
end

