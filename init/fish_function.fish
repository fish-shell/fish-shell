#
# This file defines various shellscript functions. Most of them are
# meant to be used directly by the user, but some of them, typically
# the ones whose name start with '__fish_', are only meant to be used
# internally by fish.
#

function contains -d "Test if a key is contained in a set of values"
	while set -q argv
		switch $argv[1]
			case '-h' '--h' '--he' '--hel' '--help'
				help contains
				return

			case '--'
				# End the loop, the next argument is the key
				set -e argv[1]
				break
			
			case '-*'
				printf (_ "%s: Unknown option '%s'\n") contains $argv[$i]
				help contains
				return 1

			case '*'
				# End the loop, we found the key
				break				

		end
		set -e argv[1]
	end

	if not set -q argv 
		printf (_ "%s: Key not specified\n") contains
		return 1
	end

	set -- key $argv[1]
	set -e argv[1]	

	#
	# Loop through values
	#

	printf "%s\n" $argv|grep -Fx -- $key >/dev/null
	return $status
end



#
# These are very common and useful
#
function ll -d "List contents of directory using long format"
	ls -lh $argv
end

function la -d "List contents of directory using long format, showing hidden files"
	ls -lha $argv
end


#
# Print the current working directory in a shortened form.This
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
# Make pwd print out the home directory as a tilde.
#

function pwd -d "Print working directory"
	command pwd | sed -e 's|/private||' -e "s|^$HOME|~|"
end

#
# This function is used internally by the fish command completion code
#

function __fish_describe_command -d "Command used to find descriptions for commands"
	apropos $argv | awk -v FS=" +- +" '{
		split($1, names, ", ");
		for (name in names)
			if (names[name] ~ /^'"$argv"'.* *\([18]\)/) {
				sub("\\([18]\\)", "", names[name]);
				print names[name] "\t" $2;
			}
	}'
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


#
# The following functions add support for a directory history
#

function cd -d "Change directory"

	# Skip history in subshells
	if status --is-command-substitution
		builtin cd $argv
		return $status
	end

	# Avoid set completions
	set previous (command pwd)

	if test $argv[1] = - ^/dev/null
		if test $__fish_cd_direction = next ^/dev/null
			nextd
		else
			prevd
		end
		return $status
	end
				
	builtin cd $argv[1]

	if test $status = 0 -a (command pwd) != $previous
		set -g dirprev $dirprev $previous
		set -e dirnext
		set -g __fish_cd_direction prev
	end

	return $status
end



function __bold -d "Print argument in bold"
	set_color --bold
	printf "%s" $argv[1]
	set_color normal
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

