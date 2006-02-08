#
# Make pwd print out the home directory as a tilde.
#

function pwd -d "Print working directory"
	command pwd | sed -e 's|/private||' -e "s|^$HOME|~|"
end
