#
# Make ls use colors if we are on a system that supports this
#

if command ls --version 1>/dev/null 2>/dev/null
	# This is GNU ls
	function ls --description "List contents of directory"
		set -l param --color=auto
		if isatty 1
			set param $param --indicator-style=classify
		end
		command ls $param $argv
	end

	if not set -q LS_COLORS
		if type -q -f dircolors
			set -l colorfile
			for file in ~/.dir_colors ~/.dircolors /etc/DIR_COLORS
				if test -f $file
					set colorfile $file
					break
				end
			end
			set -gx LS_COLORS (dircolors -c $colorfile | string replace -r 'setenv LS_COLORS \'(.*)\'' '$1' | string replace ' >&/dev/null'  '' )
		end
	end

else
	# BSD, OS X and a few more support colors through the -G switch instead
	if command ls -G / 1>/dev/null 2>/dev/null
		function ls --description "List contents of directory"
			command ls -G $argv
		end
	end
end

