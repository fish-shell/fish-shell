
#
# This allows us to use 'open FILENAME' to open a given file in the default
# application for the file.
#

if not test (uname) = Darwin
	function open --description "Open file in default application"
		if count $argv >/dev/null
			switch $argv[1]
				case -h --h --he --hel --help
					__fish_print_help dirh
					return 0
			end
		end

		if type -f xdg-open >/dev/null
			xdg-open $argv
		else
			mimedb -l -- $argv
		end
	end
end

