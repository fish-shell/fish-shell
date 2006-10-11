
#
# This allows us to use 'open FILENAME' to open a given file in the default
# application for the file.
#

if not test (uname) = Darwin
	function open -d (N_ "Open file in default application")
		if type -f xdg-open >/dev/null
			xdg-open $argv
		else
			mimedb -l -- $argv
		end
	end
end

