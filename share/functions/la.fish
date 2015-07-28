#
# These are very common and useful
#
function la --description "List contents of directory, including hidden files in directory using long format" --wraps ls
	command ls -lah $argv
end

