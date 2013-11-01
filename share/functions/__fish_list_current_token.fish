
#
# This function is bound to Alt-L, it is used to list the contents of
# the directory under the cursor
#

function __fish_list_current_token -d "List contents of token under the cursor if it is a directory, otherwise list the contents of the current directory"
	set val (eval echo (commandline -t))
	printf "\n"
	if test -d $val
		ls $val
	else
		set dir (dirname $val)
		if test $dir != . -a -d $dir
			ls $dir
		else
			ls
		end
	end
	commandline -f repaint
end

