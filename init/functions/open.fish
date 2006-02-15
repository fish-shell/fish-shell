
#
# This allows us to use 'open FILENAME' to open a given file in the default
# application for the file.
#

if not test (uname) = Darwin
	function open -d (_ "Open file in default application")
		mimedb -l -- $argv
	end
end

