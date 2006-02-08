#
# Make ls use colors if we are on a system that supports this
#

if ls --version 1>/dev/null 2>/dev/null
	# This is GNU ls
	function ls -d "List contents of directory"
		command ls --color=auto --indicator-style=classify $argv
	end
else
	# BSD, OS X and a few more support colors through the -G switch instead
	if ls / -G 1>/dev/null 2>/dev/null
		function ls -d "List contents of directory"
			command ls -G $argv
		end
	end
end

