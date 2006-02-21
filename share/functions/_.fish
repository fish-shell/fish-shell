
#
# Alias for gettext (or a fallback if gettext isn't installed)
#

if which gettext ^/dev/null >/dev/null
	function _ -d "Alias for the gettext command"
		gettext fish $argv
	end
else
	function _ -d "Alias for the gettext command"
		printf "%s" $argv
	end
end

