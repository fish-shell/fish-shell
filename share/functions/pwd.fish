#
# Make pwd print out the home directory as a tilde.
# Also drop '/private' directories on OS X.
#

if test (uname) = Darwin
	function pwd --description "Print working directory"
		echo $PWD | sed -e 's|/private||' -e "s|^$HOME|~|"
	end
else
	function pwd --description "Print working directory"
		echo $PWD | sed -e "s|^$HOME|~|"
	end
end