#
# Make pwd print out the home directory as a tilde.
#

function pwd -d (N_ "Print working directory")
	echo $PWD | sed -e 's|/private||' -e "s|^$HOME|~|"
end
