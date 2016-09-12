# Provide a minimalist realpath implementation to help deal with platforms that may not provide it
# as a command. If a realpath command is available simply pass all arguments thru to it. If not
# fallback to alternative solutions.

# The following is slightly subtle. The first time `realpath` is invoked this script will be read.
# If we see that there is an external command by that name we just return. That will cause fish to
# run the external command. On the other hand, if an external command isn't found we define a
# function that will provide fallback behavior.
if not command -v realpath > /dev/null
	if command -v grealpath > /dev/null
		alias realpath=grealpath
	else
		alias realpath=fish_realpath
	end
end
