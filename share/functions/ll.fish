#
# These are very common and useful
#
function ll --description "List contents of directory using long format" --wraps ls
	command ls -lh $argv
end
