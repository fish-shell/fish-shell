#
# Match colors for grep, if supported
#

if grep --color=auto --help 1>/dev/null 2>/dev/null
	if not set -q GREP_COLOR 
		set -gx GREP_COLOR '97;45'
	end
	if not set -q GREP_OPTIONS 
		set -gx GREP_OPTIONS --color=auto
	end
end

