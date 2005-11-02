#
# This file defines various functions for fish
#


function _contains_help -d "Helper function for contains"

	set bullet \*
	if count $LANG >/dev/null
		if test (expr match $LANG ".*UTF") -gt 0
			set bullet \u2022
		end
	end

	echo \tcontains - Test if a word is present in a list\n
	__bold Synopsis
	echo \n\n\tcontains \[OPTION] KEY [VALUES...]\n
	__bold Description
	echo \n\n\t$bullet (__bold -h) or (__bold --help) display help and exit\n
	echo \tTest if the set VALUES contains the string KEY.
	echo \tReturn status is 0 if yes, 1 otherwise.\n
	__bold Example
	echo \n
	echo \tfor i in \~/bin /usr/local/bin
	echo \t\tif not contains \$i \$PATH
	echo \t\t\tset PATH \$PATH i
	echo \t\tend
	echo \tend
	echo
	echo \tThe above code tests if "~/bin" and  /usr/local/bin are in the path
	echo \tand if they are not, they are added.
end

function contains -d "Test if a key is contained in a set of values"
	while set -q argv
		switch $argv[1]
			case '-h' '--h' '--he' '--hel' '--help'
				_contains_help
				return

			case '--'
				# End the loop, the next argument is the key
				set -e argv[1]
				break
			
			case '-*'
				echo Unknown option $argv[$i]
				_contains_help
				return 1

			case '*'
				# End the loop, we found the key
				break				

		end
		set -e argv[1]
	end

	if not set -q argv 
		echo "contains: Key not specified"
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
# help should use 'open' to find a suitable browser, but only
# if there is a mime database _and_ DISPLAY is set, since the
# browser will most likely be graphical. Since most systems which
# have a mime databe also have the htmlview program, this is mostly a
# theoretical problem.
#

function help -d "Show help for the fish shell"

	# Declare variables to set correct scope

	set fish_browser
	set fish_browser_bg

	#
	# Find a suitable browser for viewing the help pages. This is needed
	# by the help function defined below.
	#

	set graphical_browsers htmlview x-www-browser firefox galeon mozilla konqueror epiphany opera netscape
	set text_browsers htmlview www-browser links elinks lynx w3m

	if test $BROWSER

		# User has manualy set a preferred browser, so we respect that
		set fish_browser $BROWSER

		# If browser is known to be graphical, put into background
		if contains -- $BROWSER $graphical_browsers
			set fish_browser_bg 1
		end

	else

		# Check for a text-based browser.
		for i in $text_browsers
			if which $i 2>/dev/null >/dev/null
				set fish_browser $i
				break
			end
		end

		# If we are in a graphical environment, we check if there is a
		# graphical browser to use instead.
		if test (echo $DISPLAY)
			for i in $graphical_browsers
				if which $i 2>/dev/null >/dev/null
					set fish_browser $i
					set fish_browser_bg 1
					break
				end
			end
		end
	end

	if test -z $fish_browser
		printf "help: Could not find a web browser.\n"
		printf "Please set the variable $BROWSER to a suitable browser and try again\n\n"
		return 1
	end

	set fish_help_item $argv[1]
	set fish_help_page ""

	if test $fish_help_item = .
		set fish_help_page "builtins.html\#source"
	end

	if test $fish_help_item = difference
		set fish_help_page difference.html
	end

	if test $fish_help_item = globbing
		set fish_help_page "index.html\#expand"
	end

	if contains -- $fish_help_item (builtin -n)
		set fish_help_page "builtins.html\#"$fish_help_item
	end
	
	if contains -- $fish_help_item count dirh dirs help mimedb nextd open popd prevd pushd set_color tokenize psub umask type 
		set fish_help_page "commands.html\#"$fish_help_item
	end
	
	set idx_subj syntax completion editor job-control todo bugs history
	set idx_subj $idx_subj killring help color prompt title expand variables
	set idx_subj $idx_subj builtin-overview changes

	if contains -- $fish_help_item $idx_subj
		set fish_help_page "index.html\#"$fish_help_item
	end

	if not test $fish_help_page
		if which $fish_help_item >/dev/null ^/dev/null
			man $fish_help_item
			return
		end
		set fish_help_page "index.html"
	end

	if test $fish_browser_bg
		eval $fish_browser file://$__fish_help_dir/$fish_help_page \&
	else
		eval $fish_browser file://$__fish_help_dir/$fish_help_page
	end

end

#
# Make ls use colors if we are on a system that supports this
#
if ls --help 1>/dev/null 2>/dev/null
	function ls -d "List contents of directory"
		command ls --color=auto --indicator-style=classify $argv
	end
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
# This allows us to use 'open FILENAME' to open a given file in the default
# application for the file.
#

function open -d "Open file in default application"
	mimedb -l -- $argv
end

#
# Print the current working directory. If it is too long, it will be
# ellipsised. This function is used by the default prompt command.
#

function prompt_pwd -d "Print the current working directory, ellipsise it if it is longer than 1/4 of the terminal width"
	set wd (pwd)
	set len (echo $wd|wc -c)
	set max_width (echo $COLUMNS/4|bc)
	if test $len -gt $max_width
		#Write ellipsis character if known to be using UTF
		#else use $
		set -l ellipsis "$" #default
		if count $LANG >/dev/null
			if test (expr match $LANG ".*UTF") -gt 0
				set ellipsis \u2026
			end
		end
		printf %s%s $ellipsis (echo $wd|cut -c (echo $len-$max_width-1|bc)- ^/dev/null )
	else
		echo $wd
	end
end

#
# Make pwd print out the home directory as a tilde.
#

function pwd -d "Print working directory"
	set out (command pwd $argv)
	if echo $out| grep \^$HOME >/dev/null
		printf \~
		echo $out |cut -b (echo $HOME|wc -c)- ^/dev/null
	else
		printf "%s\n" $out
	end
end

#
# This is a neat function, stolen from zsh. It allows you to edit the
# value of a variable interactively.
#

function vared -d "Edit variable value"
	if test (count $argv) = 1
		switch $argv

			case '-h' '--h' '--he' '--hel' '--help'
				__vared_help

			case '-*'
				printf "vared: Unknown option %s\n" $argv

			case '*'
				if test (count $$argv ) -lt 2
					set init ''
					if test $$argv
						set init -- $$argv
					end
					set prompt 'set_color green; echo '$argv'; set_color normal; echo "> "'
					read -p $prompt -c $init tmp

					# If variable already exists, do not add any
					# switches, so we don't change export rules. But
					# if it does not exist, we make the variable
					# global, so that it will not die when this
					# function dies

					if test $$argv
						set -- $argv $tmp
					else
						set -g -- $argv $tmp
					end

				else

					printf "vared: %s is an array variable. Use " $argv
					set_color $fish_color_command
					printf vared
					set_color $fish_color_normal
					printf " %s[n] to edit the n:th element of %s\n" $argv $argv

				end
		end
	else
		printf "vared: Expected exactly one argument, got %s.\n\nSynopsis:\n\t" (count $argv)
		set_color $fish_color_command
		printf vared
		set_color $fish_color_normal
		printf " VARIABLE\n"
	end
end

function __vared_help -d "Display help for the vared shellscript function"

	printf "\tvared - Interactively edit the value of an environment variable\n\n"
	printf "%s\n\t%svared%s VARIABLE\n\n" (__bold Synopsis) (set_color $fish_color_command) (set_color normal)
	__bold Description
	printf "\n\n\tvared is used to interactively edit the value of an environment \n"
	printf "\tvariable. Array variables as a whole can not be edited using vared,\n" 
	printf "\tbut individual array elements can.\n\n"
	__bold Example
	printf "\n\n\t"\'"%svared%s PATH[3]"\'" edits the third element of the PATH array.\n\n" (set_color $fish_color_co\mmand) (set_color normal)
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
		echo Directory stack is empty...
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
	set -- previous (command pwd)

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


function __fish_move_last -d "Move the last element of a directory history from src to dest"
	set src $argv[1]
	set dest $argv[2]

	set size_src (count $$src)

	if test $size_src = 0
		# Cannot make this step
		echo "Hit end of history..."
		return 1
	end

	# Append current dir to the end of the destination
	set -g (echo $dest) $$dest (command pwd)

	set ssrc $$src
		
	# Change dir to the last entry in the source dir-hist
	builtin cd $ssrc[$size_src]

	# Keep all but the last from the source dir-hist 
	set -e (echo $src)[$size_src]

	# All ok, return success
	return 0
end	


function prevd -d "Move back in the directory history"
	# Parse arguments
	set show_hist 0 
	set times 1
	for i in (seq (count $argv))
		switch $argv[$i]
			case '-l'
				set show_hist 1
				continue
			case '-*'
				echo Uknown option $argv[$i]
				return 1
			case '*'
				if test $argv[$i] -ge 0 ^/dev/null
					set times $argv[$i]
				else
					echo "The number of positions to skip must be a non-negative integer"
					return 1
				end
				continue
		end
	end

	# Traverse history
	set code 1
	for i in (seq $times)
		# Try one step backward
		if __fish_move_last dirprev dirnext;
			# We consider it a success if we were able to do at least 1 step
			# (low expectations are the key to happiness ;)
			set code 0
		else
			break
		end
	end

	# Show history if needed
	if test $show_hist = 1
		dirh
	end

	# Set direction for 'cd -'
	if test $code = 0 ^/dev/null
		set -g __fish_cd_direction next
	end

	# All done
	return $code
end 


function nextd -d "Move forward in the directory history"
	# Parse arguments
	set show_hist 0 
	set times 1
	for i in (seq (count $argv))
		switch $argv[$i]
			case '-l'
				set show_hist 1
				continue
			case '-*'
				echo Uknown option $argv[$i]
				return 1
			case '*'
				if test $argv[$i] -ge 0 ^/dev/null
					set times $argv[$i]
				else
					echo "The number of positions to skip must be a non-negative integer"
					return 1
				end
				continue
		end
	end

	# Traverse history
	set code 1
	for i in (seq $times)
		# Try one step backward
		if __fish_move_last dirnext dirprev;
			# We consider it a success if we were able to do at least 1 step
			# (low expectations are the key to happiness ;)
			set code 0
		else
			break
		end
	end

	# Show history if needed
	if test $show_hist = 1
		dirh
	end

	# Set direction for 'cd -'
	if test $code = 0 ^/dev/null
		set -g __fish_cd_direction prev
	end

	# All done
	return $code
end 


function dirh -d "Print the current directory history (the back- and fwd- lists)" 
	# Avoid set comment
	set current (command pwd)
	set -- separator "  "
	set -- line_len (echo (count $dirprev) + (echo $dirprev $current $dirnext | wc -m) | bc)
	if test $line_len -gt $COLUMNS
		# Print one entry per line if history is long
		set separator "\n"
	end

	for i in $dirprev
		echo -n -e $i$separator
	end

	set_color $fish_color_history_current
	echo -n -e $current$separator
	set_color normal
	
	for i in (seq (echo (count $dirnext)) -1 1)
		echo -n -e $dirnext[$i]$separator
	end

	echo
end

function __bold -d "Print argument in bold"
	set_color --bold
	printf "%s" $argv[1]
	set_color normal
end

function __fish_type_help -d "Help for the type shellscript function"

set bullet \*
if count $LANG >/dev/null
	if test (expr match $LANG ".*UTF") -gt 0
		set bullet \u2022
	end
end

echo \ttype - Indicate how a name would be interpreted if used as a \n\tcommand name
echo
echo (__bold Synopsis)
echo
echo \t(set_color $fish_color_command)type(set_color normal) [OPTIONS] name [name ...]
echo
echo (__bold Description)
echo
echo \tWith no options, indicate how each name would be interpreted if \n\tused as a command name.  
echo
echo \t$bullet (__bold -h) or (__bold --help) print this message
echo \t$bullet (__bold -a) or (__bold --all) print all possible definitions of the specified \n\t\ \ names
echo \t$bullet (__bold -f) or (__bold --no-functions) supresses function and builtin lookup
echo \t$bullet (__bold -t) or (__bold --type) print a string which is one of alias, keyword, \n\t\ \ function, builtin, or file if name is an alias, shell \n\t\ \ reserved word, function, builtin, or disk file, respectively
echo \t$bullet (__bold -p) or (__bold --path) either return the name of the disk file that would \n\t\ \ be executed if name were specified as a command name, or nothing \n\t\ \ if (__bold "type -t name") would  not  return  file
echo \t$bullet (__bold -P) or (__bold --force-path) either return the name of the disk file that \n\t\ \ would be executed if name were specified as a command name, \n\t\ \ or nothing no file with the spacified name could be found \n\t\ \ in the PATH
echo
echo (__bold Example)
echo
echo \t\'(set_color $fish_color_command)type(set_color normal) fg\' outputs the string \'fg is a shell builtin\'.
echo

end

function type -d "Print the type of a command"

	set status 1
	set mode normal
	set selection all


	set -- shortopt -o tpPafh
	if getopt -T >/dev/null
		set longopt
	else
		set longopt -- -l type,path,force-path,all,no-functions,help
	end

	if not getopt -n type -Q $shortopt $longopt -- $argv
		return 1
	end

	set tmp -- (getopt $shortopt $longopt -- $argv)

	eval set opt -- $tmp

	for i in $opt
		switch $i
			case -t --type
				set mode type
			
			case -p --path
				set mode path
			
			case -P --force-path 
				set mode path
				set selection files
			
			case -a --all
				set selection multi

			case -f --no-functions
				set selection files

			case -h --help
				 __fish_type_help
				 return 0

			case --
				 break

		end
	end

	for i in $argv
		
		switch $i
			case '-*'
				 continue
		end

		# Found will be set to 1 if a match is found
		set found 0

		if test $selection != files

			if contains -- $i (functions -n)
				set status 0
				set found 1
				switch $mode
					case normal
						echo $i is a function with definition
						functions $i

					case type
						echo function

					case path
						 echo

				end
				if test $selection != multi
					continue
				end
			end

			if contains -- $i (builtin -n)
				set status 0
				set found 1
				switch $mode
					case normal
						echo $i is a builtin

					case type
						echo builtin

					case path
						echo
				end
				if test $selection != multi
					continue
				end
			end

		end

		if which $i ^/dev/null >/dev/null
			set status 0
			set found 1
			switch $mode
				case normal
					echo $i is (which $i)

					case type
						echo file

					case path
						which $i
			end
			if test $selection != multi
				continue
			end
		end

		if test $found = 0
			echo type: $i: not found
		end

	end

	return $status
end

function __fish_umask_parse -d "Parses a file permission specification as into an octal version"
	# Test if already a valid octal mask, and pad it with zeros
	if echo $argv | grep -E '^(0|)[0-7]{1,3}$' >/dev/null
		for i in (seq (echo 5-(echo $argv|wc -c)|bc)); set -- argv 0$argv; end
		echo $argv 
	else
		# Test if argument really is a valid symbolic mask
		if not echo $argv | grep -E '^(((u|g|o|a|)(=|\+|-)|)(r|w|x)*)(,(((u|g|o|a|)(=|\+|-)|)(r|w|x)*))*$' >/dev/null
			echo umask: Invalid mask $argv >&2
			return 1
		end

		set -e implicit_all

		# Insert inverted umask into res variable

		set tmp $umask
		for i in 1 2 3
			set -- tmp (echo $tmp|cut -c 2-)
			set -- res[$i] (echo 7-(echo $tmp|cut -c 1)|bc)
		end
				
		set -- el (echo $argv|tr , \n)
		for i in $el
			switch $i
				case 'u*'
					set idx 1
					set -- i (echo $i| cut -c 2-)

				case 'g*'
					set idx 2
					set -- i (echo $i| cut -c 2-)

				case 'o*'
					set idx 3
					set -- i (echo $i| cut -c 2-)

				case 'a*'
					set idx 1 2 3
					set -- i (echo $i| cut -c 2-)

				case '*'
					set implicit_all 1
					set idx 1 2 3
			end

			switch $i
				case '=*'
					set mode set
					set -- i (echo $i| cut -c 2-) 

				case '+*'
					set mode add
					set -- i (echo $i| cut -c 2-) 

				case '-*'
					set mode remove
					set -- i (echo $i| cut -c 2-) 

				case '*'
					if not set -q implicit_all
						echo umask: Invalid mask $argv >&2
						return
					end
					set mode set
			end

			if not echo $perm|grep -E '^(r|w|x)*$' >/dev/null
				echo umask: Invalid mask $argv >&2
				return
			end

			set val 0
			if echo $i |grep 'r' >/dev/null
				set val 4
			end
			if echo $i |grep 'w' >/dev/null
				set val (echo $val + 2|bc)
			end
			if echo $i |grep 'x' >/dev/null
				set val (echo $val + 1|bc)
			end

			for j in $idx
				switch $mode
					case set
						 set res[$j] $val

					case add
						set res[$j] (perl -e 'print( ( '$res[$j]'|'$val[$j]' )."\n" )')

					case remove
						set res[$j] (perl -e 'print( ( (7-'$res[$j]')&'$val[$j]' )."\n" )')
				end
			end
		end

		for i in 1 2 3
			set res[$i] (echo 7-$res[$i]|bc)
		end
		echo 0$res[1]$res[2]$res[3]
	end
end

function __fish_umask_print_symbolic
	set -l res ""
	set -l letter a u g o

	for i in 2 3 4
		set res $res,$letter[$i]=
		set val (echo $umask|cut -c $i)

		if contains $val 0 1 2 3
		   set res {$res}r
		end
	
		if contains $val 0 1 4 5
		   set res {$res}w
		end

		if contains $val 0 2 4 6
		   set res {$res}x
		end

	end

	echo $res|cut -c 2-
end

function umask -d "Set default file permission mask"

	set -l as_command 0
	set -l symbolic 0

	set -- shortopt -o pSh
	if getopt -T >/dev/null
		set longopt
	else
		set longopt -- -l as-command,symbolic,help
	end

	if not getopt -n umask -Q $shortopt $longopt -- $argv
		return 1
	end

	set tmp -- (getopt $shortopt $longopt -- $argv)

	eval set opt -- $tmp

	while count $opt >/dev/null

		switch $opt[1]
			case -h --help
				help umask
				return 0

			case -p --as-command
				set as_command 1				 

			case -S --symbolic
				set symbolic 1

			case --
				set -e opt[1]
				break

		end

		set -e opt[1]

	end

	switch (count $opt)

		case 0
			if not set -q umask
				set -g umask 113
			end
			if test $as_command -eq 1
				echo umask $umask
			else
				if test $symbolic -eq 1
					__fish_umask_print_symbolic $umask
				else
					echo $umask
				end
			end

		case 1
			set -l parsed (__fish_umask_parse $opt)
			if test (count $parsed) -eq 1
				set -g umask $parsed
			end

		case '*'
			echo umask: Too may arguments >&2

	end

end


function psub -d "Read from stdin into a file and output the filename. Remove the file when the command that calles psub exits."

	set -l filename
	set -l funcname

	if count $argv >/dev/null
		switch $argv[1]
			case '-h*' --h --he --hel --help

				help psub
				return 0

			case '*'
				echo psub: Unknown argument $argv[1]
				return 1
		end
	end

	if not status --is-command-substitution
		echo psub: Not inside of command substitution >&2
		return
	end

	# Find unique file name for writing output to
	while true
		set filename /tmp/.psub.(echo %self).(random);
		if not test -e $filename
			break;
		end
	end

	# Write output to pipe. This needs to be done in the background so
	# that the command substitution exits without needing to wait for
	# all the commands to exit
	mkfifo $filename 
	cat >$filename &

	# Write filename to stdout
	echo $filename

	# Find unique function name
	while true
		set funcname __fish_psub_(random);
		if not functions $funcname >/dev/null ^/dev/null
			break;
		end
	end

	# Make sure we erase file when caller exits
	eval function $funcname --on-job-exit caller\; rm $filename\; functions -e $funcname\; end	

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
