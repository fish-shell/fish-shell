#
# Make pwd print out the home directory as a tilde.
# Also drop '/private' directories on OS X.
#

if test (uname) = Darwin
	function pwd -d (N_ "Print working directory")
		echo $PWD | sed -e 's|/private||' -e "s|^$HOME|~|"
	end
else
	function pwd -d (N_ "Print working directory")
		echo $PWD | sed -e "s|^$HOME|~|"
	end
end