
#
# This function is bound to Alt-L, it is used to list the contents of
# the directory under the cursor
#

function __fish_list_current_token -d "List contents of token under the cursor if it is a directory, otherwise list the contents of the current directory"
	set val (eval echo (commandline -t))
	printf "\n"
	if test -d $val
		for f in $val/*
			echo $f
		end
	else
		set dir (dirname $val)
		if test $dir != . -a -d $dir
			for f in $dir/*
				echo $f
			end
		else
			for f in *
				echo $f
			end
		end
	end
	commandline -f repaint
end

