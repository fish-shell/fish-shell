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
# help should use 'open' to find a suitable browser, but only
# if there is a mime database _and_ DISPLAY is set, since the
# browser will most likely be graphical. Since most systems which
# have a mime databe also have the htmlview program, this is mostly a
# theoretical problem.
#

function help -d "Show help for the fish shell"

	# Declare variables to set correct scope
	set -l fish_browser
	set -l fish_browser_bg

	set -l h syntax completion editor job-control todo bugs history killring help
	set h $h color prompt title variables builtin-overview changes expand 
	set h $h expand-variable expand-home expand-brace expand-wildcard 
	set -l help_topics $h expand-command-substitution expand-process 

	#
	# Find a suitable browser for viewing the help pages. This is needed
	# by the help function defined below.
	#
	set -l graphical_browsers htmlview x-www-browser firefox galeon mozilla konqueror epiphany opera netscape
	set -l text_browsers htmlview www-browser links elinks lynx w3m

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

		# If we are in a graphical environment, check if there is a graphical
		# browser to use instead.
		if test "$DISPLAY" -a \( "$XAUTHORITY" = "$HOME/.Xauthority" -o "$XAUTHORITY" = "" \)
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
		printf (_ '%s: Could not find a web browser.\n') help
		printf (_ 'Please set the variable $BROWSER to a suitable browser and try again\n\n')
		return 1
	end

	set fish_help_item $argv[1]

	switch "$fish_help_item"
		case ""
			set fish_help_page index.html
		case "."
			set fish_help_page "builtins.html\#source"
		case difference
			set fish_help_page difference.html
		case globbing
			set fish_help_page "index.html\#expand"
		case (builtin -n)
			set fish_help_page "builtins.html\#$fish_help_item"
		case contains count dirh dirs help mimedb nextd open popd prevd pushd set_color tokenize psub umask type vared
			set fish_help_page "commands.html\#$fish_help_item"
		case $help_topics
			set fish_help_page "index.html\#$fish_help_item"
		case "*"
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

if ls --version 1>/dev/null 2>/dev/null
	# This is GNU ls
	function ls -d "List contents of directory"
		command ls --color=auto --indicator-style=classify $argv
	end
else
	# BSD, OS X and a few more support colors through the -G switch instead
	if ls / -G 1>/dev/null 2>/dev/null
		function ls -d "List contents of directory"
			command ls -G $argv
		end
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

if not test (uname) = Darwin
	function open -d "Open file in default application"
		mimedb -l -- $argv
	end
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
# This is a neat function, stolen from zsh. It allows you to edit the
# value of a variable interactively.
#

function vared -d "Edit variable value"
	if test (count $argv) = 1
		switch $argv

			case '-h' '--h' '--he' '--hel' '--help'
				help vared

			case '-*'
				printf (_ "%s: Unknown option %s\n") vared $argv

			case '*'
				if test (count $$argv ) -lt 2
					set init ''
					if test $$argv
						set -- init $$argv
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

					printf (_ '%s: %s is an array variable. Use %svared%s %s[n] to edit the n:th element of %s\n') $argv (set_color $fish_color_command) (set_color $fish_color_normal) vared $argv $argv
				end
		end
	else
		printf (_ '%s: Expected exactly one argument, got %s.\n\nSynopsis:\n\t%svared%s VARIABLE\n') vared (count $argv) (set_color $fish_color_command) (set_color $fish_color_normal)
	end
end

#
# This function is used internally by the fish command completion code
#

function __fish_describe_command -d "Command used to find descriptions for commands"
	apropos $argv | awk -v FS=" +- +" '{split($1, names, ", "); for (name in names) if (names[name] ~ /^'"$argv"'.* *\([18]\)/) { sub("\\([18]\\)", "", names[name]); print names[name] "\t" $2; } }'
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


function __fish_move_last -d "Move the last element of a directory history from src to dest"
	set -l src $argv[1]
	set -l dest $argv[2]

	set -l size_src (count $$src)

	if test $size_src = 0
		# Cannot make this step
		printf (_ "Hit end of history...\n")
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
	set -l show_hist 0 
	set -l times 1
	for i in (seq (count $argv))
		switch $argv[$i]
			case '-l' --l --li --lis --list
				set show_hist 1
				continue
			case '-*'
				printf (_ "%s: Unknown option %s\n" ) prevd $argv[$i]
				return 1
			case '*'
				if test $argv[$i] -ge 0 ^/dev/null
					set times $argv[$i]
				else
					printf (_ "The number of positions to skip must be a non-negative integer\n")
					return 1
				end
				continue
		end
	end

	# Traverse history
	set -l code 1
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
	set -l show_hist 0 
	set -l times 1
	for i in (seq (count $argv))
		switch $argv[$i]
			case '-l' --l --li --lis --list
				set show_hist 1
				continue
			case '-*'
				printf (_ "%s: Unknown option %s\n" ) nextd $argv[$i]
				return 1
			case '*'
				if test $argv[$i] -ge 0 ^/dev/null
					set times $argv[$i]
				else
					printf (_ "%s: The number of positions to skip must be a non-negative integer\n" ) nextd
					return 1
				end
				continue
		end
	end

	# Traverse history
	set -l code 1
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
	set -l current (command pwd)
	set -l separator "  "
	set -l line_len (echo (count $dirprev) + (echo $dirprev $current $dirnext | wc -m) | bc)
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


function __trap_translate_signal
	set upper (echo $argv[1]|tr a-z A-Z)
	if expr $upper : 'SIG.*' >/dev/null
		echo $upper | cut -c 4-
	else
		echo $upper
	end
end

function __trap_switch

	switch $argv[1]
		case EXIT
			echo --on-exit %self
		
		case '*'
			echo --on-signal $argv[1]
	end	

end

function trap -d 'Perform an action when the shell recives a signal'

	set -l mode
	set -l cmd 
	set -l sig 
	set -l shortopt
	set -l longopt

	set -l shortopt -o lph
	set -l longopt
	if not getopt -T >/dev/null
		set longopt -l print,help,list-signals
	end

	if not getopt -n type -Q $shortopt $longopt -- $argv
		return 1
	end

	set -l tmp (getopt $shortopt $longopt -- $argv)

	eval set opt $tmp

	while count $opt >/dev/null
		switch $opt[1]
			case -h --help
				help trap
				return 0
			
			case -p --print
				set mode print
			
			case -l --list-signals
				set mode list
			
			case --
				 set -e opt[1]
				 break

		end
		set -e opt[1]
	end

	if not count $mode >/dev/null

		switch (count $opt)

			case 0
				set mode print

			case 1
				set mode clear

			case '*'
				if test opt[1] = -
					set -e opt[1]
					set mode clear
				else
					set mode set
				end
		end
	end

	switch $mode
		case clear
			for i in $opt
				set -- sig (__trap_translate_signal $i)
				if test $sig
					functions -e __trap_handler_$sig				
				end
			end

		case set
			set -l cmd $opt[1]
			set -e opt[1]

			for i in $opt

				set -l -- sig (__trap_translate_signal $i)
				set -- sw (__trap_switch $sig)

				if test $sig
					eval "function __trap_handler_$sig $sw; $cmd; end"
				else
					return 1
				end
			end

		case print
			set -l names 

			if count $opt >/dev/null
				set -- names $opt
			else
				set -- names (functions -na|grep "^__trap_handler_"|sed -e 's/__trap_handler_//' )
			end

			for i in $names

				set -- sig (__trap_translate_signal $i)

				if test sig
					functions __trap_handler_$i
				else
					return 1
				end

			end

		case list
			kill -l
			
	end

end


function type -d "Print the type of a command"

	# Initialize
	set -l status 1
	set -l mode normal
	set -l selection all

	#
	# Get options
	#
	set -l shortopt -o tpPafh
	set -l longopt
	if not getopt -T >/dev/null
		set longopt -l type,path,force-path,all,no-functions,help
	end

	if not getopt -n type -Q $shortopt $longopt -- $argv
		return 1
	end

	set -l tmp (getopt $shortopt $longopt -- $argv)

	set -l opt
	eval set opt $tmp

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
				 help type
				 return 0

			case --
				 break

		end
	end

	# Check all possible types for the remaining arguments
	for i in $argv
		
		switch $i
			case '-*'
				 continue
		end

		# Found will be set to 1 if a match is found
		set found 0

		if test $selection != files

			if contains -- $i (functions -na)
				set status 0
				set found 1
				switch $mode
					case normal
						printf (_ '%s is a function with definition\n') $i
						functions $i

					case type
						printf (_ 'function\n')

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
						printf (_ '%s is a builtin\n') $i

					case type
						printf (_ 'builtin\n')

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
					printf (_ '%s is %s\n') $i (which $i)

					case type
						printf (_ 'file\n')

					case path
						which $i
			end
			if test $selection != multi
				continue
			end
		end

		if test $found = 0
			printf (_ "%s: Could not find '%s'") type $i
		end

	end

	return $status
end

function __fish_umask_parse -d "Parses a file permission specification as into an octal version"
	# Test if already a valid octal mask, and pad it with zeros
	if echo $argv | grep -E '^(0|)[0-7]{1,3}$' >/dev/null
		for i in (seq (echo 5-(echo $argv|wc -c)|bc)); set argv 0$argv; end
		echo $argv 
	else
		# Test if argument really is a valid symbolic mask
		if not echo $argv | grep -E '^(((u|g|o|a|)(=|\+|-)|)(r|w|x)*)(,(((u|g|o|a|)(=|\+|-)|)(r|w|x)*))*$' >/dev/null
			printf (_ "%s: Invalid mask '%s'\n") umask $argv >&2
			return 1
		end

		set -l implicit_all

		# Insert inverted umask into res variable

		set -l mode
		set -l val
		set -l tmp $umask
		set -l res

		for i in 1 2 3
			set tmp (echo $tmp|cut -c 2-)
			set res[$i] (echo 7-(echo $tmp|cut -c 1)|bc)
		end
				
		set -l el (echo $argv|tr , \n)
		for i in $el
			switch $i
				case 'u*'
					set idx 1
					set i (echo $i| cut -c 2-)

				case 'g*'
					set idx 2
					set i (echo $i| cut -c 2-)

				case 'o*'
					set idx 3
					set i (echo $i| cut -c 2-)

				case 'a*'
					set idx 1 2 3
					set i (echo $i| cut -c 2-)

				case '*'
					set implicit_all 1
					set idx 1 2 3
			end

			switch $i
				case '=*'
					set mode set
					set i (echo $i| cut -c 2-) 

				case '+*'
					set mode add
					set i (echo $i| cut -c 2-) 

				case '-*'
					set mode remove
					set i (echo $i| cut -c 2-) 

				case '*'
					if not count $implicit_all >/dev/null
						printf (_ "%s: Invalid mask %s\n") umask $argv >&2
						return
					end
					set mode set
			end

			if not echo $perm|grep -E '^(r|w|x)*$' >/dev/null
				printf (_ "%s: Invalid mask %s\n") umask $argv >&2
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

	set -l shortopt -o pSh
	set -l longopt
	if not getopt -T >/dev/null
		set longopt -l as-command,symbolic,help
	end

	if not getopt -n umask -Q $shortopt $longopt -- $argv
		return 1
	end

	set -l tmp (getopt $shortopt $longopt -- $argv)

	eval set opt $tmp

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
				return 0
			end
			return 1

		case '*'
			printf (_ '%s: Too many arguments\n') umask >&2

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
				printf (_ "%s: Unknown argument '%s'\n") psub $argv[1]
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

