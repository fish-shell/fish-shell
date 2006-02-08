
#
# Print the current working directory in a shortened form. This
# function is used by the default prompt command.
#

if test (uname) = Darwin
	function prompt_pwd -d "Print the current working directory, shortend to fit the prompt"
		if test "$PWD" != "$HOME"
			printf "%s" (echo $PWD|sed -e 's|/private||' -e "s|^$HOME|~|" -e 's-/\([^/]\)\([^/]*\)-/\1-g')
			echo $PWD|sed -e 's-.*/[^/]\([^/]*$\)-\1-'
		else
			echo '~'
		end
	end
else
	function prompt_pwd -d "Print the current working directory, shortend to fit the prompt"
		if test "$PWD" != "$HOME"
			printf "%s" (echo $PWD|sed -e "s|^$HOME|~|" -e 's-/\([^/]\)\([^/]*\)-/\1-g')
			echo $PWD|sed -e 's-.*/[^/]\([^/]*$\)-\1-'
		else
			echo '~'
		end
	end
end


#
# This function is bound to Alt-L, it is used to list the contents of
# the directory under the cursor
#

function __fish_list_current_token -d "List contents of token under the cursor if it is a directory, otherwise list the contents of the current directory"
	set val (eval echo (commandline -t))
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
end


function pushd -d "Push directory to stack"
	# Comment to avoid set completions
	set -g dirstack (command pwd) $dirstack
	cd $argv[1]
end


function popd -d "Pop dir from stack"
	if test $dirstack[1]
		cd $dirstack[1]
	else
		printf (_ "%s: Directory stack is empty...") popd
		return 1
	end

	set -e dirstack[1]

end


function dirs -d "Print directory stack"
	echo -n (command pwd)"  "
	for i in $dirstack
		echo -n $i"  "
	end
	echo
end


function prevd-or-backward-word --key-binding 
	if test -z (commandline)
		prevd
	else
		commandline -f backward-word
	end
end

function nextd-or-forward-word --key-binding 
	if test -z (commandline)
		nextd
	else
		commandline -f forward-word
	end
end

#
# This function deletes a character from the commandline if it is
# non-empty, and exits the shell otherwise. Implementing this
# functionality has been a longstanding request from various
# fish-users.
#

function delete-or-exit --key-binding 
	if test (commandline)
		commandline -f delete-char
	else
		exit
	end
end

